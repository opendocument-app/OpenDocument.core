#ifndef ODR_OPENDOCUMENTFILE_H
#define ODR_OPENDOCUMENTFILE_H

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
class OpenDocumentFile final {
public:
    OpenDocumentFile();
    ~OpenDocumentFile();

    bool open(const Path &);
    bool decrypt(const std::string &);
    void close();

    bool isOpen() const;
    bool isDecrypted() const;
    const FileMeta &getMeta() const;
    bool isFile(const Path &) const;

    std::string loadEntry(const Path &);
    std::unique_ptr<tinyxml2::XMLDocument> loadXml(const Path &);

    // TODO hacky
    const ZipReader &getZipReader() const;

private:
    class Impl;
    const std::unique_ptr<Impl> impl;
};

}

#endif //ODR_OPENDOCUMENTFILE_H
