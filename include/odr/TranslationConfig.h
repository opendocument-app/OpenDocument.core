#ifndef ODR_TRANSLATIONCONFIG_H
#define ODR_TRANSLATIONCONFIG_H

#include <cstdint>

namespace odr {

struct TranslationConfig {
    // starting sheet for spreadsheet, starting page for presentation, ignored for text, ignored for graphics
    std::uint32_t entryOffset = 0;
    // translate only N sheets / pages; zero means translate all
    std::uint32_t entryCount = 0;
    // translate presentation notes
    bool translateNotes = true;

    // embed js, embed css, embed images, squash up tables in spreadsheet or pages in presentation
    bool singleOutput = true;
    // generate js or not
    bool useJavaScript = true;
    // general limit for any repeating thing
    std::uint32_t maxRepeat = 1000;
    // limit for table dimensions (surrounds problems with repeat* in spreadsheet)
    std::uint32_t maxTableRows = 10000;
    std::uint32_t maxTableCols = 10000;

    bool editable = false;
};

}

#endif //ODR_TRANSLATIONCONFIG_H
