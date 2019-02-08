#ifndef OPENDOCUMENT_OPENDOCUMENTFILE_H
#define OPENDOCUMENT_OPENDOCUMENTFILE_H

#include <memory>
#include <string>
#include <map>
#include "miniz_zip.h"
#include "tinyxml2.h"

namespace opendocument {

class OpenDocumentFile final {
public:
    struct Entry {
        std::size_t size;
        std::size_t size_compressed;
        mz_uint index;
        std::string mediaType;
    };

    struct Meta {
        typedef int Version;
        enum class Type {
            UNKNOWN,
            TEXT,
            SPREADSHEET,
            PRESENTATION
        };
        struct Text {
            std::size_t pageCount;
        };
        struct Spreadsheet {
            struct Table {
                std::string name;
                std::size_t rowCount;
                std::size_t columnCount;
            };

            std::size_t tableCount;
            Table *tables;
        };
        struct Presentation {
            std::size_t pageCount;
        };

        Type type = Type::UNKNOWN;

        union {
            Text text;
            Spreadsheet spreadsheet;
            Presentation presentation;
        };
    };

    typedef std::map<std::string, Entry> Entries;

    static const std::map<std::string, Meta::Type> MIMETYPES;

    explicit OpenDocumentFile(const std::string &);
    OpenDocumentFile(const OpenDocumentFile &) = delete;
    ~OpenDocumentFile();
    OpenDocumentFile &operator=(const OpenDocumentFile &) = delete;

    const Entries getEntries() const;
    const Meta &getMeta() const;
    bool isFile(const std::string &) const;
    std::string loadText(const std::string &);
    std::unique_ptr<tinyxml2::XMLDocument> loadXML(const std::string &);

    void close();

private:
    bool createEntries();
    bool createMeta();
    void destroyMeta();

    mz_zip_archive _zip;
    Meta _meta;
    Entries _entries;
};

}

#endif //OPENDOCUMENT_OPENDOCUMENTFILE_H
