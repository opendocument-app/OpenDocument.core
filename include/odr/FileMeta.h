#ifndef ODR_FILEMETA_H
#define ODR_FILEMETA_H

#include <cstdint>
#include <string>
#include <list>
#include "FileType.h"

namespace odr {

struct FileMeta {
    struct Entry {
        std::string name;
        std::size_t rowCount = 0;
        std::size_t columnCount = 0;
    };

    FileType type = FileType::UNKNOWN;
    bool encrypted;
    std::size_t entryCount;
    std::list<Entry> entries;
};

}

#endif //ODR_FILEMETA_H
