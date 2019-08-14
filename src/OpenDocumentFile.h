#ifndef ODR_OPENDOCUMENTFILE_H
#define ODR_OPENDOCUMENTFILE_H

#include <cstdint>
#include <memory>
#include <string>
#include <map>
#include "odr/FileMeta.h"

namespace tinyxml2 {
class XMLDocument;
}

namespace odr {

#ifdef ODR_CRYPTO
enum class ChecksumType {
    UNKNOWN, SHA1, SHA1_1K, SHA256, SHA256_1K
};
enum class AlgorithmType {
    UNKNOWN, AES256_CBC, TRIPLE_DES_CBC, BLOWFISH_CFB
};
enum class KeyDerivationType {
    UNKNOWN, PBKDF2
};
#endif

struct OpenDocumentEntry {
    std::string path;
    std::size_t size_real;
    std::size_t size_uncompressed;
    std::size_t size_compressed;
    uint32_t index;
    std::string mediaType;
    bool encrypted;

#ifdef ODR_CRYPTO
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
#endif
};

class OpenDocumentFileImpl;

class OpenDocumentFile final {
public:
    typedef std::map<std::string, OpenDocumentEntry> Entries;

    OpenDocumentFile();
    ~OpenDocumentFile();

    bool open(const std::string &);
    bool decrypt(const std::string &);
    void close();

    bool isOpen() const;
    bool isDecrypted() const;
    const Entries &getEntries() const;
    const FileMeta &getMeta() const;
    bool isFile(const std::string &) const;

    std::string loadEntry(const std::string &);
    std::unique_ptr<tinyxml2::XMLDocument> loadXML(const std::string &);

private:
    OpenDocumentFileImpl * const impl_;
};

}

#endif //ODR_OPENDOCUMENTFILE_H
