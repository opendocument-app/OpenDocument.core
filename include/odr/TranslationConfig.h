#ifndef ODR_TRANSLATIONCONFIG_H
#define ODR_TRANSLATIONCONFIG_H

#include <cstdint>

namespace odr {

struct TranslationConfig {
    // e.g. split up tables in spreadsheet or pages in presentation
    bool oneHtml = false;
    // embed images in html
    bool embedMedia = false;
    // general limit for any repeating thing
    uint32_t maxRepeat = 1000;
    // limit for table dimensions (surrounds problems with repeat* in spreadsheet)
    uint32_t maxTableRows = 10000;
    uint32_t maxTableCols = 10000;
};

}

#endif //ODR_TRANSLATIONCONFIG_H
