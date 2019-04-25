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

enum class ChecksumType {
    UNKNOWN, SHA1, SHA1_1K, SHA256, SHA256_1K
};
enum class AlgorithmType {
    UNKNOWN, AES256_CBC, TRIPLE_DES_CBC, BLOWFISH_CFB
};
enum class KeyDerivationType {
    UNKNOWN, PBKDF2
};

struct OpenDocumentEntry {
    std::string path;
    std::size_t size_real;
    std::size_t size_uncompressed;
    std::size_t size_compressed;
    uint32_t index;
    std::string mediaType;
    bool encrypted;

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

class OpenDocumentFile {
public:
    typedef std::map<std::string, OpenDocumentEntry> Entries;

    static std::unique_ptr<OpenDocumentFile> create();

    virtual ~OpenDocumentFile() = default;

    virtual bool open(const std::string &) = 0;
    virtual bool decrypt(const std::string &) = 0;
    virtual void close() = 0;

    virtual bool isOpen() const = 0;
    virtual bool isDecrypted() const = 0;
    virtual const Entries getEntries() const = 0;
    virtual const FileMeta &getMeta() const = 0;
    virtual bool isFile(const std::string &) const = 0;

    virtual std::unique_ptr<std::string> loadEntry(const std::string &) = 0;
    virtual std::unique_ptr<tinyxml2::XMLDocument> loadXML(const std::string &) = 0;
};

}

#endif //ODR_OPENDOCUMENTFILE_H
