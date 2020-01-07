#ifndef ODR_MICROSOFTCONTENTTRANSLATOR_H
#define ODR_MICROSOFTCONTENTTRANSLATOR_H

#include <memory>

namespace tinyxml2 {
class XMLElement;
}

namespace odr {

struct TranslationContext;

class MicrosoftContentTranslator final {
public:
    static void translate(const tinyxml2::XMLElement &in, TranslationContext &context);
};

}

#endif //ODR_MICROSOFTCONTENTTRANSLATOR_H
