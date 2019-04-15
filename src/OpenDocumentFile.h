#ifndef ODR_OPENDOCUMENTFILE_H
#define ODR_OPENDOCUMENTFILE_H

#include <cstdint>
#include <memory>
#include <string>
#include <map>
#include "odr/DocumentMeta.h"

namespace tinyxml2 {
class XMLDocument;
}

namespace odr {

class OpenDocumentFile {
public:
    struct Entry {
        std::size_t size_real;
        std::size_t size_uncompressed;
        std::size_t size_compressed;
        uint32_t index;
        std::string mediaType;

        bool encrypted;
        std::string checksumType;
        std::string checksum;
        std::string algorithmName;
        std::string initialisationVector;
        std::string keyDerivationName;
        std::uint64_t keySize;
        std::uint64_t keyIterationCount;
        std::string keySalt;
        std::string startKeyGenerationName;
        std::string startKeySize;
    };

    typedef std::map<std::string, Entry> Entries;

    static std::unique_ptr<OpenDocumentFile> create();

    virtual ~OpenDocumentFile() = default;

    virtual bool open(const std::string &) = 0;
    virtual bool decrypt(const std::string &) = 0;
    virtual void close() = 0;

    virtual const Entries getEntries() const = 0;
    virtual const DocumentMeta &getMeta() const = 0;
    virtual bool isFile(const std::string &) const = 0;

    virtual std::string loadText(const std::string &) = 0;
    virtual std::unique_ptr<tinyxml2::XMLDocument> loadXML(const std::string &) = 0;
};

}

#endif //ODR_OPENDOCUMENTFILE_H
