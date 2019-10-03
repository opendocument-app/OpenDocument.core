#ifndef ODR_TRANSLATIONCONTEXT_H
#define ODR_TRANSLATIONCONTEXT_H

#include <memory>
#include <string>
#include <list>
#include <vector>
#include <unordered_map>
#include <iostream>

namespace tinyxml2 {
class XMLDocument;
class XMLElement;
class XMLText;
}

namespace odr {

struct TranslationConfig;
struct OpenDocumentFile;
struct MicrosoftOpenXmlFile;
struct FileMeta;

struct TranslationContext {
    const TranslationConfig *config;
    OpenDocumentFile *odFile;
    MicrosoftOpenXmlFile *msFile;
    const FileMeta *meta;
    std::unique_ptr<tinyxml2::XMLDocument> content;
    std::unique_ptr<tinyxml2::XMLDocument> sharedStringsDoc;
    std::vector<const tinyxml2::XMLElement *> sharedStrings;
    std::unordered_map<std::string, std::list<std::string>> styleDependencies;
    std::ostream *output;

    std::uint32_t currentTextTranslationIndex;
    std::unordered_map<std::uint32_t, const tinyxml2::XMLText *> textTranslation;
};

}

#endif //ODR_TRANSLATIONCONTEXT_H
