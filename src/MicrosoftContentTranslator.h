#ifndef ODR_MICROSOFTCONTENTTRANSLATOR_H
#define ODR_MICROSOFTCONTENTTRANSLATOR_H

#include <memory>

namespace tinyxml2 {
class XMLElement;
}

namespace odr {

struct TranslationContext;

class MicrosoftContentTranslator {
public:
    static std::unique_ptr<MicrosoftContentTranslator> create();

    virtual ~MicrosoftContentTranslator() = default;
    virtual void translate(const tinyxml2::XMLElement &in, TranslationContext &context) const = 0;
};

}

#endif //ODR_MICROSOFTCONTENTTRANSLATOR_H
