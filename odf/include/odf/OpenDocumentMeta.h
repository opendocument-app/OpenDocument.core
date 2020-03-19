#ifndef ODR_OPENDOCUMENTMETA_H
#define ODR_OPENDOCUMENTMETA_H

#include "access/Path.h"
#include <exception>
#include <string>
#include <unordered_map>

namespace tinyxml2 {
class XMLDocument;
}

namespace odr {

struct FileMeta;
class Storage;

class NoOpenDocumentFileException : public std::exception {
public:
  const char *what() const noexcept override {
    return "not a open document file";
  }

private:
};

namespace OpenDocumentMeta {

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
  std::unordered_map<Path, Entry> entries;

  std::uint64_t smallestFileSize;
  const Path *smallestFilePath;
  const Entry *smallestFileEntry;
};

extern FileMeta parseFileMeta(Storage &storage, bool decrypted);

extern Manifest parseManifest(Storage &storage);
extern Manifest parseManifest(const tinyxml2::XMLDocument &manifest);

} // namespace OpenDocumentMeta

} // namespace odr

#endif // ODR_OPENDOCUMENTMETA_H
