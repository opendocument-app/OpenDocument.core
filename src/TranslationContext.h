#ifndef ODR_TRANSLATIONCONTEXT_H
#define ODR_TRANSLATIONCONTEXT_H

#include <string>
#include <list>
#include <unordered_map>
#include <iostream>

namespace tinyxml2 {
class XMLDocument;
class XMLElement;
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
    const tinyxml2::XMLDocument *styles;
    const tinyxml2::XMLDocument *content;
    std::unordered_map<std::string, std::list<std::string>> styleDependencies;
    const tinyxml2::XMLElement *currentElement;
    std::ostream *output;
};

}

#endif //ODR_TRANSLATIONCONTEXT_H
