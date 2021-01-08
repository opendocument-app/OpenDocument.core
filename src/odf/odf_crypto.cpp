#include <abstract/storage.h>
#include <crypto/crypto_util.h>
#include <odf/odf_crypto.h>
#include <odr/exceptions.h>
#include <sstream>
#include <util/stream_util.h>

namespace odr::odf {

bool can_decrypt(const Manifest::Entry &entry) noexcept {
  return entry.checksumType != ChecksumType::UNKNOWN &&
         entry.algorithm != AlgorithmType::UNKNOWN &&
         entry.keyDerivation != KeyDerivationType::UNKNOWN &&
         entry.startKeyGeneration != ChecksumType::UNKNOWN;
}

std::string hash(const std::string &input, const ChecksumType checksumType) {
  switch (checksumType) {
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

std::string decrypt(const std::string &input, const std::string &derivedKey,
                    const std::string &initialisationVector,
                    const AlgorithmType algorithm) {
  switch (algorithm) {
  case AlgorithmType::AES256_CBC:
    return crypto::Util::decryptAES(derivedKey, initialisationVector, input);
  case AlgorithmType::TRIPLE_DES_CBC:
    return crypto::Util::decryptTripleDES(derivedKey, initialisationVector,
                                          input);
  case AlgorithmType::BLOWFISH_CFB:
    return crypto::Util::decryptBlowfish(derivedKey, initialisationVector,
                                         input);
  default:
    throw std::invalid_argument("algorithm");
  }
}

std::string start_key(const Manifest::Entry &entry,
                      const std::string &password) {
  const std::string result = hash(password, entry.startKeyGeneration);
  if (result.size() < entry.startKeySize)
    throw std::invalid_argument("hash too small");
  return result.substr(0, entry.startKeySize);
}

std::string derive_key_and_decrypt(const Manifest::Entry &entry,
                                   const std::string &startKey,
                                   const std::string &input) {
  const std::string derivedKey = crypto::Util::pbkdf2(
      entry.keySize, startKey, entry.keySalt, entry.keyIterationCount);
  return decrypt(input, derivedKey, entry.initialisationVector,
                 entry.algorithm);
}

bool validate_password(const Manifest::Entry &entry,
                       std::string decrypted) noexcept {
  try {
    const std::size_t padding = crypto::Util::padding(decrypted);
    decrypted = decrypted.substr(0, decrypted.size() - padding);
    const std::string checksum = hash(decrypted, entry.checksumType);
    return checksum == entry.checksum;
  } catch (...) {
    return false;
  }
}

namespace {
class CryptoOpenDocumentFile : public abstract::ReadStorage {
public:
  const std::shared_ptr<abstract::ReadStorage> parent;
  const Manifest manifest;
  const std::string startKey;

  CryptoOpenDocumentFile(std::shared_ptr<abstract::ReadStorage> parent,
                         Manifest manifest, std::string startKey)
      : parent(std::move(parent)), manifest(std::move(manifest)),
        startKey(std::move(startKey)) {}

  bool isSomething(const common::Path &p) const final {
    return parent->isSomething(p);
  }
  bool isFile(const common::Path &p) const final {
    return parent->isSomething(p);
  }
  bool isDirectory(const common::Path &p) const final {
    return parent->isSomething(p);
  }
  bool isReadable(const common::Path &p) const final { return isFile(p); }

  std::uint64_t size(const common::Path &p) const final {
    const auto it = manifest.entries.find(p);
    if (it == manifest.entries.end())
      return parent->size(p);
    return it->second.size;
  }

  void visit(Visitor v) const final { parent->visit(v); }

  std::unique_ptr<std::istream> read(const common::Path &path) const final {
    const auto it = manifest.entries.find(path);
    if (it == manifest.entries.end())
      return parent->read(path);
    if (!can_decrypt(it->second))
      throw UnsupportedCryptoAlgorithm();
    // TODO stream
    auto source = parent->read(path);
    const std::string input = util::stream::read(*source);
    std::string result = crypto::Util::inflate(
        derive_key_and_decrypt(it->second, startKey, input));
    return std::make_unique<std::istringstream>(std::move(result));
  }
};
} // namespace

bool decrypt(std::shared_ptr<abstract::ReadStorage> &storage,
             const Manifest &manifest, const std::string &password) {
  if (!manifest.encrypted)
    return true;
  if (!can_decrypt(*manifest.smallestFileEntry))
    throw UnsupportedCryptoAlgorithm();
  const std::string startKey =
      odf::start_key(*manifest.smallestFileEntry, password);
  const std::string input =
      util::stream::read(*storage->read(*manifest.smallestFilePath));
  const std::string decrypt =
      derive_key_and_decrypt(*manifest.smallestFileEntry, startKey, input);
  if (!validate_password(*manifest.smallestFileEntry, decrypt))
    return false;
  storage = std::make_shared<CryptoOpenDocumentFile>(std::move(storage),
                                                     manifest, startKey);
  return true;
}

} // namespace odr::odf
