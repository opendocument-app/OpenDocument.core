#ifndef ODR_TRANSLATIONCONTEXT_H
#define ODR_TRANSLATIONCONTEXT_H

#include <memory>
#include <string>
#include <list>
#include <vector>
#include <unordered_map>
#include <iostream>
#include "TableLocation.h"

namespace tinyxml2 {
class XMLDocument;
class XMLElement;
class XMLText;
}

namespace odr {

struct TranslationConfig;
class Storage;
struct FileMeta;

struct TranslationContext {
    const TranslationConfig *config;
    const FileMeta *meta;

    // input files
    Storage *storage;

    // input xml
    std::unique_ptr<tinyxml2::XMLDocument> style;
    std::unique_ptr<tinyxml2::XMLDocument> content;
    std::unordered_map<std::string, std::list<std::string>> odStyleDependencies; // odf
    std::unique_ptr<tinyxml2::XMLDocument> msSharedStringsDocument; // xlsx
    std::vector<const tinyxml2::XMLElement *> msSharedStrings; // xlsx

    // current
    tinyxml2::XMLNode *currentStyleNode;
    tinyxml2::XMLNode *currentContentNode;
    std::uint32_t currentEntry;
    std::uint32_t currentTableRowStart;
    std::uint32_t currentTableRowEnd;
    std::uint32_t currentTableColStart;
    std::uint32_t currentTableColEnd;
    TableLocation currentTableLocation;
    std::unordered_map<std::uint32_t, std::string> odDefaultCellStyles;

    // output
    std::ostream *output;

    // editing
    std::uint32_t currentTextTranslationIndex;
    std::unordered_map<std::uint32_t, const tinyxml2::XMLText *> textTranslation;
};

}

#endif //ODR_TRANSLATIONCONTEXT_H
