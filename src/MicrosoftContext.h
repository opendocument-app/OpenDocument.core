#ifndef ODR_MICROSOFTCONTEXT_H
#define ODR_MICROSOFTCONTEXT_H

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
struct MicrosoftOpenXmlFile;
struct FileMeta;

struct MicrosoftContext {
    const TranslationConfig *config;
    MicrosoftOpenXmlFile *file;
    const FileMeta *meta;
    const tinyxml2::XMLDocument *styles;
    const tinyxml2::XMLDocument *content;
    std::unordered_map<std::string, std::list<std::string>> styleDependencies;
    const tinyxml2::XMLElement *currentElement;
    std::ostream *output;
};

}

#endif //ODR_MICROSOFTCONTEXT_H
