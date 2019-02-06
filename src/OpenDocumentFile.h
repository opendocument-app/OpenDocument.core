#ifndef OPENDOCUMENT_OPENDOCUMENTFILE_H
#define OPENDOCUMENT_OPENDOCUMENTFILE_H

#include <string>
#include "Storage.h"

namespace opendocument {

class OpenDocumentFile : public Storage {
public:
    struct Meta {
        typedef int Version;
        enum class Type {
            TEXT,
            SPREADSHEET,
            PRESENTATION
        };
        struct Text {
            size_t pageCount;
        };
        struct Spreadsheet {
            struct Table {
                std::string name;
                size_t rowCount;
                size_t columnCount;
            };

            size_t tableCount;
            Table *tables;
        };
        struct Presentation {
            size_t pageCount;
        };

        size_t size;
        Type type;
        Version version;

        union {
            Text text;
            Spreadsheet spreadsheet;
            Presentation presentation;
        };
    };

    explicit OpenDocumentFile(Storage &access);
    OpenDocumentFile(const OpenDocumentFile &) = delete;
    ~OpenDocumentFile() override;
    OpenDocumentFile &operator=(const OpenDocumentFile &) = delete;

    bool exists(const Path &) override;
    bool isFile(const Path &) override;
    bool isDirectory(const Path &) override;
    Size getSize(const Path &) override;
    std::unique_ptr<Source> read(const Path &) override;
    void close() override;

    const Meta &getMeta() const;
    void loadXML(const Path &path) const;

private:
    void createMeta();
    void destroyMeta();

    Storage &_access;
    Meta _meta;
};

}

#endif //OPENDOCUMENT_OPENDOCUMENTFILE_H
