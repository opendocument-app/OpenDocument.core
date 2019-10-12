#ifndef ODR_FILEMETA_H
#define ODR_FILEMETA_H

#include <cstdint>
#include <string>
#include <vector>

namespace odr {

enum class FileType {
    UNKNOWN,
    // https://en.wikipedia.org/wiki/OpenDocument
    OPENDOCUMENT_TEXT,
    OPENDOCUMENT_PRESENTATION,
    OPENDOCUMENT_SPREADSHEET,
    OPENDOCUMENT_GRAPHICS,
    // https://en.wikipedia.org/wiki/Office_Open_XML
    OFFICE_OPEN_XML_DOCUMENT,
    OFFICE_OPEN_XML_PRESENTATION,
    OFFICE_OPEN_XML_WORKBOOK,
    // https://en.wikipedia.org/wiki/List_of_Microsoft_Office_filename_extensions
    LEGACY_WORD_DOCUMENT,
    LEGACY_POWERPOINT_PRESENTATION,
    LEGACY_EXCEL_WORKSHEETS,
    // https://en.wikipedia.org/wiki/PDF
    PORTABLE_DOCUMENT_FORMAT,
    // https://en.wikipedia.org/wiki/Text_file
    TEXT_FILE,
    // https://en.wikipedia.org/wiki/Comma-separated_values
    COMMA_SEPARATED_VALUES,
    // https://en.wikipedia.org/wiki/Rich_Text_Format
    RICH_TEXT_FORMAT,
    // https://en.wikipedia.org/wiki/Markdown
    MARKDOWN,
};

struct FileMeta {
    struct Entry {
        std::string name;
        std::uint32_t rowCount = 0;
        std::uint32_t columnCount = 0;
        std::string notes;
    };

    FileType type = FileType::UNKNOWN;
    bool encrypted;
    std::uint32_t entryCount;
    std::vector<Entry> entries;
};

}

#endif //ODR_FILEMETA_H
