#ifndef ODR_MICROSOFTOPENXMLFILE_H
#define ODR_MICROSOFTOPENXMLFILE_H

#include <cstdint>
#include <memory>
#include <string>
#include <map>
#include "odr/FileMeta.h"

namespace tinyxml2 {
class XMLDocument;
}

namespace odr {

struct MicrosoftOpenXmlEntry {
    std::string path;
    std::size_t size_real;
    std::size_t size_uncompressed;
    std::size_t size_compressed;
    uint32_t index;
    std::string mediaType;
    bool encrypted;
};

class MicrosoftOpenXmlFileImpl;

class MicrosoftOpenXmlFile final {
public:
    typedef std::map<std::string, MicrosoftOpenXmlEntry> Entries;

    MicrosoftOpenXmlFile();
    ~MicrosoftOpenXmlFile();

    bool open(const std::string &);
    bool decrypt(const std::string &);
    void close();

    bool isOpen() const;
    bool isDecrypted() const;
    const Entries getEntries() const;
    const FileMeta &getMeta() const;
    bool isFile(const std::string &) const;

    std::unique_ptr<std::string> loadEntry(const std::string &);
    std::unique_ptr<tinyxml2::XMLDocument> loadXML(const std::string &);

private:
    MicrosoftOpenXmlFileImpl * const impl_;
};

}

#endif //ODR_MICROSOFTOPENXMLFILE_H
