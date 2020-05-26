#ifndef ODR_ODF_META_H
#define ODR_ODF_META_H

#include <access/Path.h>
#include <exception>
#include <string>
#include <unordered_map>

namespace pugi {
class xml_document;
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
    std::size_t size{0};

    ChecksumType checksumType{ChecksumType::UNKNOWN};
    std::string checksum;
    AlgorithmType algorithm{AlgorithmType::UNKNOWN};
    std::string initialisationVector{0};
    KeyDerivationType keyDerivation{KeyDerivationType::UNKNOWN};
    std::uint64_t keySize{0};
    std::uint64_t keyIterationCount{0};
    std::string keySalt{0};
    ChecksumType startKeyGeneration{ChecksumType::UNKNOWN};
    std::uint64_t startKeySize{0};
  };

  bool encrypted{false};
  std::unordered_map<access::Path, Entry> entries;

  std::uint64_t smallestFileSize{0};
  const access::Path *smallestFilePath{nullptr};
  const Entry *smallestFileEntry{nullptr};
};

FileMeta parseFileMeta(const access::ReadStorage &storage, bool decrypted);

Manifest parseManifest(const access::ReadStorage &storage);
Manifest parseManifest(const pugi::xml_document &manifest);
} // namespace Meta

} // namespace odf
} // namespace odr

#endif // ODR_ODF_META_H
