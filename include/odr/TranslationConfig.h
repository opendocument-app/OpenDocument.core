#ifndef ODR_TRANSLATIONCONFIG_H
#define ODR_TRANSLATIONCONFIG_H

#include <cstdint>

namespace odr {

struct TranslationConfig {
    // starting sheet for spreadsheet, starting page for presentation, ignored for text, ignored for graphics
    std::uint32_t entryOffset = 0;
    // translate only N sheets / pages; zero means translate all
    std::uint32_t entryCount = 0;
    // create editable output
    bool editable = false;

    // spreadsheet table offset
    std::uint32_t tableOffsetRows = 0;
    std::uint32_t tableOffsetCols = 0;
    // spreadsheet table limit
    std::uint32_t tableLimitRows = 10000;
    std::uint32_t tableLimitCols = 500;
    bool tableLimitByDimensions = true;
};

}

#endif //ODR_TRANSLATIONCONFIG_H
