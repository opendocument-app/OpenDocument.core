#include <odr/internal/iwork/iwork_crypto.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/crypto/crypto_util.hpp>
#include <odr/internal/util/byte_string.hpp>
#include <odr/internal/util/stream_util.hpp>

#include <array>
#include <stdexcept>
#include <utility>

namespace odr::internal::iwork {

namespace {

constexpr std::string_view verifier_path = "/.iwpv2";
constexpr std::string_view hint_path = "/.iwph";

/// Members that stay plain; every other member is encrypted.
constexpr std::array<std::string_view, 2> plain_members = {
    "/Metadata/Properties.plist", "/Metadata/DocumentIdentifier"};

constexpr std::size_t aes_block_size = 16;
constexpr std::size_t aes_key_size = 16;
constexpr std::size_t hmac_size = 20;
/// The fixtures use 100000. The cap keeps a crafted count from blocking for
/// hours, as `max_spin_count` does for OOXML.
constexpr std::uint32_t max_iterations = 10000000;
/// A member longer than one segment is split into segments of 1 MiB of
/// payload each: IV, random block, payload, a whole padding block, HMAC.
constexpr std::size_t segment_payload_size = std::size_t{1} << 20;
constexpr std::size_t segment_size = aes_block_size + aes_block_size +
                                     segment_payload_size + aes_block_size +
                                     hmac_size;

/// Payload of one segment.
std::string decrypt_segment(const std::string_view segment,
                            const std::string_view key,
                            const std::string_view password) {
  if (segment.size() < aes_block_size + 2 * aes_block_size + hmac_size ||
      (segment.size() - aes_block_size - hmac_size) % aes_block_size != 0) {
    throw std::runtime_error("iwork: bad encrypted segment size");
  }
  const std::string_view iv = segment.substr(0, aes_block_size);
  const std::string_view ciphertext = segment.substr(
      aes_block_size, segment.size() - aes_block_size - hmac_size);
  const std::string_view mac = segment.substr(segment.size() - hmac_size);

  const std::string plaintext =
      internal::crypto::util::decrypt_aes_cbc(key, iv, ciphertext);
  const auto padding = static_cast<std::uint8_t>(plaintext.back());
  if (padding == 0 || padding > aes_block_size ||
      plaintext.find_first_not_of(static_cast<char>(padding),
                                  plaintext.size() - padding) !=
          std::string::npos) {
    throw std::runtime_error("iwork: bad padding, the member is damaged");
  }
  std::string payload = plaintext.substr(
      aes_block_size, plaintext.size() - aes_block_size - padding);
  if (internal::crypto::util::hmac_sha1(password, payload) != mac) {
    throw std::runtime_error("iwork: bad HMAC, the member is damaged");
  }
  return payload;
}

/// Whether a member starts with a full segment, which ends in a whole block of
/// padding. A large member can also be a single segment.
bool is_segmented(const std::string_view data, const std::string_view key) {
  if (data.size() <= segment_size) {
    return false;
  }
  const std::size_t last_block = segment_size - hmac_size - aes_block_size;
  const std::string block = internal::crypto::util::decrypt_aes_cbc(
      key, data.substr(last_block - aes_block_size, aes_block_size),
      data.substr(last_block, aes_block_size));
  return block == std::string(aes_block_size, '\x10');
}

bool is_plain(const AbsPath &path) {
  for (const std::string_view member : plain_members) {
    if (path.string() == member) {
      return true;
    }
  }
  return false;
}

bool is_hidden(const AbsPath &path) {
  return path.string() == verifier_path || path.string() == hint_path;
}

/// The package with its members decrypted when opened.
class DecryptedPackage final : public abstract::ReadableFilesystem {
public:
  DecryptedPackage(std::shared_ptr<abstract::ReadableFilesystem> filesystem,
                   std::string key, std::string password)
      : m_filesystem{std::move(filesystem)}, m_key{std::move(key)},
        m_password{std::move(password)} {}

  [[nodiscard]] bool exists(const AbsPath &path) const override {
    return !is_hidden(path) && m_filesystem->exists(path);
  }
  [[nodiscard]] bool is_file(const AbsPath &path) const override {
    return !is_hidden(path) && m_filesystem->is_file(path);
  }
  [[nodiscard]] bool is_directory(const AbsPath &path) const override {
    return m_filesystem->is_directory(path);
  }

  [[nodiscard]] std::unique_ptr<abstract::FileWalker>
  file_walker(const AbsPath &path) const override {
    return m_filesystem->file_walker(path);
  }

  [[nodiscard]] std::shared_ptr<abstract::File>
  open(const AbsPath &path) const override {
    if (is_hidden(path)) {
      return nullptr;
    }
    std::shared_ptr<abstract::File> file = m_filesystem->open(path);
    if (file == nullptr || is_plain(path)) {
      return file;
    }
    return std::make_shared<MemoryFile>(
        decrypt_member(util::stream::read(*file->stream()), m_key, m_password));
  }

private:
  std::shared_ptr<abstract::ReadableFilesystem> m_filesystem;
  std::string m_key;
  std::string m_password;
};

} // namespace

} // namespace odr::internal::iwork

namespace odr::internal {

bool iwork::password_encrypted(const abstract::ReadableFilesystem &filesystem) {
  return filesystem.is_file(AbsPath(verifier_path));
}

FileType iwork::guess_encrypted_file_type(
    const abstract::ReadableFilesystem &filesystem) {
  bool tables = false;
  for (const auto walker = filesystem.file_walker(AbsPath("/Index"));
       !walker->end(); walker->next()) {
    const std::string path = walker->path().string();
    if (path.starts_with("/Index/Slide") ||
        path.starts_with("/Index/TemplateSlide")) {
      return FileType::iwork_keynote;
    }
    tables = tables || path.starts_with("/Index/Tables/");
  }
  return tables ? FileType::iwork_numbers : FileType::iwork_pages;
}

std::optional<std::shared_ptr<abstract::ReadableFilesystem>>
iwork::decrypt(std::shared_ptr<abstract::ReadableFilesystem> filesystem,
               const std::string_view password) {
  const std::string verifier =
      util::stream::read(*filesystem->open(AbsPath(verifier_path))->stream());
  if (verifier.size() != 104) {
    throw std::runtime_error("iwork: bad .iwpv2 size");
  }
  util::byte_string::Reader reader(verifier);
  const auto version = reader.read<std::uint16_t>();
  const auto format = reader.read<std::uint16_t>();
  const auto iterations = reader.read<std::uint32_t>();
  if (version != 2 || format != 1) {
    throw UnsupportedCryptoAlgorithm();
  }
  if (iterations == 0 || iterations > max_iterations) {
    throw std::runtime_error("iwork: bad .iwpv2 iteration count");
  }
  const std::string_view rest = reader.rest();
  const std::string_view salt = rest.substr(0, 16);
  const std::string_view iv = rest.substr(16, aes_block_size);
  const std::string_view check = rest.substr(32, 64);

  std::string key =
      internal::crypto::util::pbkdf2(aes_key_size, password, salt, iterations);
  const std::string block =
      internal::crypto::util::decrypt_aes_cbc(key, iv, check);
  if (internal::crypto::util::sha256(block.substr(0, 32)) != block.substr(32)) {
    return std::nullopt;
  }
  return std::make_shared<DecryptedPackage>(
      std::move(filesystem), std::move(key), std::string(password));
}

std::string iwork::decrypt_member(const std::string_view data,
                                  const std::string_view key,
                                  const std::string_view password) {
  if (!is_segmented(data, key)) {
    return decrypt_segment(data, key, password);
  }
  std::string result;
  for (std::size_t offset = 0; offset < data.size(); offset += segment_size) {
    result += decrypt_segment(data.substr(offset, segment_size), key, password);
  }
  return result;
}

} // namespace odr::internal
