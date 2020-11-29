#ifndef ODR_ODF_MANIFEST_H
#define ODR_ODF_MANIFEST_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <access/Path.h>

namespace pugi {
class xml_document;
}

namespace odr::access {
class ReadStorage;
} // namespace odr

namespace odr::odf {

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

Manifest parseManifest(const pugi::xml_document &manifest);

}

#endif // ODR_ODF_MANIFEST_H
