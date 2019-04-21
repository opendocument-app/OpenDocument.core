#ifndef ODR_FILETYPE_H
#define ODR_FILETYPE_H

namespace odr {

enum class FileType {
    UNKNOWN,
    // https://en.wikipedia.org/wiki/OpenDocument
    OPENDOCUMENT_TEXT,
    OPENDOCUMENT_PRESENTATION,
    OPENDOCUMENT_SPREADSHEET,
    OPENDOCUMENT_GRAPHICS
};

}

#endif //ODR_FILETYPE_H
