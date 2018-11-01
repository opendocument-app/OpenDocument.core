#ifndef OPENDOCUMENT_OPENDOCUMENTFILE_H
#define OPENDOCUMENT_OPENDOCUMENTFILE_H

#include <string>
#include "InputOutput.h"

namespace opendocument {

class OpenDocumentFile : public ReadableStorage {
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
                size_t dimension[2];
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

    OpenDocumentFile(ReadableStorage &access);
    ~OpenDocumentFile() override;

    const Meta &meta() const;

    size_t size(const Path &path) const override;
    Source &read(const Path &path) override;
    void loadXML(const Path &path) const;
    void close() override;
private:
    ReadableStorage &_access;
    Meta _meta;

    void createMeta();
    void destroyMeta();
};

}

#endif //OPENDOCUMENT_OPENDOCUMENTFILE_H
