#ifndef ODR_DOCUMENTMETA_H
#define ODR_DOCUMENTMETA_H

#include <cstdint>
#include <string>
#include <list>

namespace odr {

enum class DocumentType {
    UNKNOWN,
    TEXT,
    SPREADSHEET,
    PRESENTATION,
    GRAPHICS
};

struct DocumentMeta {
    typedef int Version;
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
        std::list<Table> tables;
    };
    struct Presentation {
        std::size_t pageCount;
    };

    DocumentMeta() {}
    ~DocumentMeta() {}

    DocumentType type = DocumentType::UNKNOWN;

    union {
        Text text;
        Spreadsheet spreadsheet;
        Presentation presentation;
    };
};

}

#endif //ODR_DOCUMENTMETA_H
