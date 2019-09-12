#ifndef ODR_MICROSOFTOPENXMLFILE_H
#define ODR_MICROSOFTOPENXMLFILE_H

#include <memory>
#include <string>

namespace tinyxml2 {
class XMLDocument;
}

namespace odr {

struct FileMeta;
class Path;
class ZipReader;

// TODO storage
class MicrosoftOpenXmlFile final {
public:
    MicrosoftOpenXmlFile();
    ~MicrosoftOpenXmlFile();

    bool open(const Path &);
    bool decrypt(const std::string &);
    void close();

    bool isOpen() const;
    bool isDecrypted() const;
    const FileMeta &getMeta() const;
    bool isFile(const Path &) const;

    const Path &getContentPath() const;

    std::string loadEntry(const Path &);
    std::unique_ptr<tinyxml2::XMLDocument> loadXml(const Path &);
    std::unique_ptr<tinyxml2::XMLDocument> loadRelationships(const Path &);
    std::unique_ptr<tinyxml2::XMLDocument> loadContent();

    // TODO hacky
    const ZipReader &getZipReader() const;

private:
    class Impl;
    const std::unique_ptr<Impl> impl;
};

}

#endif //ODR_MICROSOFTOPENXMLFILE_H
