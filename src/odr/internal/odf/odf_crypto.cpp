#include <odr/internal/odf/odf_crypto.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/crypto/crypto_util.hpp>
#include <odr/internal/util/stream_util.hpp>
#include <odr/internal/zip/zip_archive.hpp>
#include <odr/internal/zip/zip_file.hpp>

#include <iostream>
#include <stdexcept>

namespace odr::internal {

bool odf::can_decrypt(const Manifest::Entry &entry) noexcept {
  return (!entry.checksum.has_value() ||
          (entry.checksum->type != ChecksumType::UNKNOWN)) &&
         (entry.algorithm.type != AlgorithmType::UNKNOWN) &&
         (entry.key_derivation.type != KeyDerivationType::UNKNOWN) &&
         (entry.start_key_generation.type != ChecksumType::UNKNOWN);
}

std::string odf::hash(const std::string &input,
                      const ChecksumType checksum_type) {
  switch (checksum_type) {
  case ChecksumType::SHA256:
    return crypto::util::sha256(input);
  case ChecksumType::SHA1:
    return crypto::util::sha1(input);
  case ChecksumType::SHA256_1K:
    return crypto::util::sha256(input.substr(0, 1024));
  case ChecksumType::SHA1_1K:
    return crypto::util::sha1(input.substr(0, 1024));
  default:
    throw std::invalid_argument("checksum type");
  }
}

std::string odf::decrypt(const std::string &input,
                         const std::string &derived_key,
                         const std::string &initialisation_vector,
                         const AlgorithmType algorithm) {
  switch (algorithm) {
  case AlgorithmType::AES256_CBC:
    return crypto::util::decrypt_aes_cbc(derived_key, initialisation_vector,
                                         input);
  case AlgorithmType::TRIPLE_DES_CBC:
    return crypto::util::decrypt_triple_des(derived_key, initialisation_vector,
                                            input);
  case AlgorithmType::BLOWFISH_CFB:
    return crypto::util::decrypt_blowfish(derived_key, initialisation_vector,
                                          input);
  case AlgorithmType::AES256_GCM:
    return crypto::util::decrypt_aes_gcm(derived_key, initialisation_vector,
                                         input);
  default:
    throw std::invalid_argument("algorithm");
  }
}

std::string odf::start_key(const Manifest::Entry &entry,
                           const std::string &password) {
  const std::string result = hash(password, entry.start_key_generation.type);
  if (result.size() < entry.start_key_generation.size) {
    throw std::invalid_argument("hash too small");
  }
  return result.substr(0, entry.start_key_generation.size);
}

std::string odf::derive_key(const Manifest::Entry &entry,
                            const std::string &start_key) {
  switch (entry.key_derivation.type) {
  case KeyDerivationType::PBKDF2:
    return crypto::util::pbkdf2(entry.key_derivation.size, start_key,
                                entry.key_derivation.salt,
                                entry.key_derivation.iteration_count);
  case KeyDerivationType::ARGON2ID:
    return crypto::util::argon2id(
        entry.key_derivation.size, start_key, entry.key_derivation.salt,
        entry.key_derivation.iteration_count,
        entry.key_derivation.argon2_memory, entry.key_derivation.argon2_lanes);
  default:
    throw std::invalid_argument("key derivation type");
  }
}

std::string odf::derive_key_and_decrypt(const Manifest::Entry &entry,
                                        const std::string &start_key,
                                        const std::string &input) {
  std::string derived_key = derive_key(entry, start_key);
  return decrypt(input, derived_key, entry.algorithm.initialisation_vector,
                 entry.algorithm.type);
}

bool odf::validate_password(const Manifest::Entry &entry,
                            std::string decrypted) noexcept {
  if (!entry.checksum.has_value()) {
    return false;
  }

  try {
    const std::size_t padding = crypto::util::padding(decrypted);
    decrypted = decrypted.substr(0, decrypted.size() - padding);
    const std::string checksum = hash(decrypted, entry.checksum->type);
    return checksum == entry.checksum->value;
  } catch (...) {
    // TODO why catch all?
    return false;
  }
}

namespace odf {
namespace {
class DecryptedFilesystem final : public abstract::ReadableFilesystem {
public:
  DecryptedFilesystem(std::shared_ptr<abstract::ReadableFilesystem> parent,
                      Manifest manifest, std::string start_key)
      : m_parent(std::move(parent)), m_manifest(std::move(manifest)),
        m_start_key(std::move(start_key)) {}

  [[nodiscard]] bool exists(const common::Path &path) const final {
    return m_parent->exists(path);
  }

  [[nodiscard]] bool is_file(const common::Path &path) const final {
    return m_parent->is_file(path);
  }

  [[nodiscard]] bool is_directory(const common::Path &path) const final {
    return m_parent->is_directory(path);
  }

  [[nodiscard]] std::unique_ptr<abstract::FileWalker>
  file_walker(const common::Path &path) const final {
    return m_parent->file_walker(path);
  }

  [[nodiscard]] std::shared_ptr<abstract::File>
  open(const common::Path &path) const final {
    const auto it = m_manifest.entries.find(path);
    if (it == std::end(m_manifest.entries)) {
      return m_parent->open(path);
    }
    if (!can_decrypt(it->second)) {
      throw UnsupportedCryptoAlgorithm();
    }
    // TODO stream
    auto source = m_parent->open(path)->stream();
    const std::string input = util::stream::read(*source);
    std::string result = crypto::util::inflate(
        derive_key_and_decrypt(it->second, m_start_key, input));
    return std::make_shared<common::MemoryFile>(result);
  }

private:
  const std::shared_ptr<abstract::ReadableFilesystem> m_parent;
  const Manifest m_manifest;
  const std::string m_start_key;
};
} // namespace
} // namespace odf

std::shared_ptr<abstract::ReadableFilesystem>
odf::decrypt(const std::shared_ptr<abstract::ReadableFilesystem> &filesystem,
             const Manifest &manifest, const std::string &password) {
  if (!manifest.encrypted) {
    return nullptr;
  }

  if (auto it = manifest.entries.find(common::Path("encrypted-package"));
      it != std::end(manifest.entries)) {
    try {
      const std::string start_key = odf::start_key(it->second, password);
      const std::string input =
          util::stream::read(*filesystem->open(it->first)->stream());
      std::string decrypt = crypto::util::inflate(
          derive_key_and_decrypt(it->second, start_key, input));

      auto memory_file =
          std::make_shared<common::MemoryFile>(std::move(decrypt));
      return zip::ZipFile(memory_file).archive()->filesystem();
    } catch (...) {
      return nullptr;
    }
  }

  try {
    const auto &smallest_file_path = manifest.smallest_file_path;
    const auto &smallest_file_entry = manifest.smallest_file_entry();
    if (!can_decrypt(smallest_file_entry)) {
      throw UnsupportedCryptoAlgorithm();
    }
    const std::string start_key = odf::start_key(smallest_file_entry, password);
    // TODO stream decrypt
    const std::string input =
        util::stream::read(*filesystem->open(smallest_file_path)->stream());
    const std::string decrypt =
        derive_key_and_decrypt(smallest_file_entry, start_key, input);
    if (!validate_password(smallest_file_entry, decrypt)) {
      return nullptr;
    }
    return std::make_shared<DecryptedFilesystem>(filesystem, manifest,
                                                 start_key);
  } catch (...) {
    return nullptr;
  }
}

} // namespace odr::internal
