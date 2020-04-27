#ifndef ODR_ODF_META_H
#define ODR_ODF_META_H

#include <access/Path.h>
#include <exception>
#include <string>
#include <unordered_map>

namespace tinyxml2 {
class XMLDocument;
}

namespace odr {
struct FileMeta;

namespace access {
class ReadStorage;
} // namespace access
} // namespace odr

namespace odr {
namespace odf {

struct NoOpenDocumentFileException final : public std::exception {
  const char *what() const noexcept final {
    return "not an open document file";
  }
};

namespace Meta {
enum class ChecksumType { UNKNOWN, SHA256, SHA1, SHA256_1K, SHA1_1K };
enum class AlgorithmType { UNKNOWN, AES256_CBC, TRIPLE_DES_CBC, BLOWFISH_CFB };
enum class KeyDerivationType { UNKNOWN, PBKDF2 };

struct Manifest {
  struct Entry {
    std::size_t size;

    ChecksumType checksumType = ChecksumType::UNKNOWN;
    std::string checksum;
    AlgorithmType algorithm = AlgorithmType::UNKNOWN;
    std::string initialisationVector;
    KeyDerivationType keyDerivation = KeyDerivationType::UNKNOWN;
    std::uint64_t keySize;
    std::uint64_t keyIterationCount;
    std::string keySalt;
    ChecksumType startKeyGeneration = ChecksumType::UNKNOWN;
    std::uint64_t startKeySize;
  };

  bool encrypted;
  std::unordered_map<access::Path, Entry> entries;

  std::uint64_t smallestFileSize;
  const access::Path *smallestFilePath;
  const Entry *smallestFileEntry;
};

FileMeta parseFileMeta(const access::ReadStorage &storage, bool decrypted);

Manifest parseManifest(const access::ReadStorage &storage);
Manifest parseManifest(const tinyxml2::XMLDocument &manifest);
} // namespace Meta

} // namespace odf
} // namespace odr

#endif // ODR_ODF_META_H
