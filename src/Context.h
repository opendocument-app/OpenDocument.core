#ifndef ODR_CONTEXT_H
#define ODR_CONTEXT_H

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
struct DocumentMeta;

// TODO: input/output context?

struct Context {
    const TranslationConfig *config;
    OpenDocumentFile *file;
    const DocumentMeta *meta;
    const tinyxml2::XMLDocument *styles;
    const tinyxml2::XMLDocument *content;
    std::unordered_map<std::string, std::list<std::string>> styleDependencies;
    const tinyxml2::XMLElement *currentElement;
    std::ostream *output;
};

}

#endif //ODR_CONTEXT_H
