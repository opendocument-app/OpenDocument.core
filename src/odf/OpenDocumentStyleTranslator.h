#ifndef ODR_OPENDOCUMENTSTYLETRANSLATOR_H
#define ODR_OPENDOCUMENTSTYLETRANSLATOR_H

#include <memory>

namespace tinyxml2 {
class XMLElement;
}

namespace odr {

struct TranslationContext;

class OpenDocumentStyleTranslator final {
public:
    static std::string escapeStyleName(const std::string &);
    static void translate(const tinyxml2::XMLElement &in, TranslationContext &context);
};

}

#endif //ODR_OPENDOCUMENTSTYLETRANSLATOR_H
