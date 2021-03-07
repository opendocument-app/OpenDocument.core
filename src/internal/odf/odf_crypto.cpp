#include <internal/abstract/file.h>
#include <internal/abstract/filesystem.h>
#include <internal/common/file.h>
#include <internal/crypto/crypto_util.h>
#include <internal/odf/odf_crypto.h>
#include <internal/util/stream_util.h>
#include <odr/exceptions.h>
#include <sstream>

namespace odr::internal::odf {

bool can_decrypt(const Manifest::Entry &entry) noexcept {
  return entry.checksum_type != ChecksumType::UNKNOWN &&
         entry.algorithm != AlgorithmType::UNKNOWN &&
         entry.key_derivation != KeyDerivationType::UNKNOWN &&
         entry.start_key_generation != ChecksumType::UNKNOWN;
}

std::string hash(const std::string &input, const ChecksumType checksum_type) {
  switch (checksum_type) {
  case ChecksumType::SHA256:
    return crypto::Util::sha256(input);
  case ChecksumType::SHA1:
    return crypto::Util::sha1(input);
  case ChecksumType::SHA256_1K:
    return crypto::Util::sha256(input.substr(0, 1024));
  case ChecksumType::SHA1_1K:
    return crypto::Util::sha1(input.substr(0, 1024));
  default:
    throw std::invalid_argument("checksumType");
  }
}

std::string decrypt(const std::string &input, const std::string &derived_key,
                    const std::string &initialisation_vector,
                    const AlgorithmType algorithm) {
  switch (algorithm) {
  case AlgorithmType::AES256_CBC:
    return crypto::Util::decrypt_AES(derived_key, initialisation_vector, input);
  case AlgorithmType::TRIPLE_DES_CBC:
    return crypto::Util::decrypt_TripleDES(derived_key, initialisation_vector,
                                           input);
  case AlgorithmType::BLOWFISH_CFB:
    return crypto::Util::decrypt_Blowfish(derived_key, initialisation_vector,
                                          input);
  default:
    throw std::invalid_argument("algorithm");
  }
}

std::string start_key(const Manifest::Entry &entry,
                      const std::string &password) {
  const std::string result = hash(password, entry.start_key_generation);
  if (result.size() < entry.start_key_size) {
    throw std::invalid_argument("hash too small");
  }
  return result.substr(0, entry.start_key_size);
}

std::string derive_key_and_decrypt(const Manifest::Entry &entry,
                                   const std::string &start_key,
                                   const std::string &input) {
  const std::string derivedKey = crypto::Util::pbkdf2(
      entry.key_size, start_key, entry.key_salt, entry.key_iteration_count);
  return decrypt(input, derivedKey, entry.initialisation_vector,
                 entry.algorithm);
}

bool validate_password(const Manifest::Entry &entry,
                       std::string decrypted) noexcept {
  try {
    const std::size_t padding = crypto::Util::padding(decrypted);
    decrypted = decrypted.substr(0, decrypted.size() - padding);
    const std::string checksum = hash(decrypted, entry.checksum_type);
    return checksum == entry.checksum;
  } catch (...) {
    return false;
  }
}

namespace {
class CryptoOpenDocumentFile final : public abstract::ReadableFilesystem {
public:
  CryptoOpenDocumentFile(std::shared_ptr<abstract::ReadableFilesystem> parent,
                         Manifest manifest, std::string start_key)
      : m_parent(std::move(parent)), m_manifest(std::move(manifest)),
        m_start_key(std::move(start_key)) {}

  [[nodiscard]] bool exists(common::Path p) const final {
    return m_parent->exists(std::move(p));
  }

  [[nodiscard]] bool is_file(common::Path p) const final {
    return m_parent->is_file(std::move(p));
  }

  [[nodiscard]] bool is_directory(common::Path p) const final {
    return m_parent->is_directory(std::move(p));
  }

  [[nodiscard]] std::unique_ptr<abstract::FileWalker>
  file_walker(common::Path path) const final {
    return m_parent->file_walker(path);
  }

  [[nodiscard]] std::shared_ptr<abstract::File>
  open(common::Path path) const final {
    const auto it = m_manifest.entries.find(path);
    if (it == m_manifest.entries.end()) {
      return m_parent->open(path);
    }
    if (!can_decrypt(it->second)) {
      throw UnsupportedCryptoAlgorithm();
    }
    // TODO stream
    auto source = m_parent->open(path)->read();
    const std::string input = util::stream::read(*source);
    std::string result = crypto::Util::inflate(
        derive_key_and_decrypt(it->second, m_start_key, input));
    return std::make_shared<common::MemoryFile>(result);
  }

private:
  const std::shared_ptr<abstract::ReadableFilesystem> m_parent;
  const Manifest m_manifest;
  const std::string m_start_key;
};
} // namespace

bool decrypt(std::shared_ptr<abstract::ReadableFilesystem> &storage,
             const Manifest &manifest, const std::string &password) {
  if (!manifest.encrypted) {
    return true;
  }
  if (!can_decrypt(*manifest.smallest_file_entry)) {
    throw UnsupportedCryptoAlgorithm();
  }
  const std::string startKey =
      odf::start_key(*manifest.smallest_file_entry, password);
  // TODO stream decrypt
  const std::string input =
      util::stream::read(*storage->open(*manifest.smallest_file_path)->read());
  const std::string decrypt =
      derive_key_and_decrypt(*manifest.smallest_file_entry, startKey, input);
  if (!validate_password(*manifest.smallest_file_entry, decrypt)) {
    return false;
  }
  storage = std::make_shared<CryptoOpenDocumentFile>(std::move(storage),
                                                     manifest, startKey);
  return true;
}

} // namespace odr::internal::odf
