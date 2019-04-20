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
    struct Entry {
        std::string name;
        std::size_t rowCount = 0;
        std::size_t columnCount = 0;
    };

    DocumentType type = DocumentType::UNKNOWN;
    std::string mediaType;
    std::string version;
    bool encrypted;
    std::size_t entryCount;
    std::list<Entry> entries;
};

}

#endif //ODR_DOCUMENTMETA_H
