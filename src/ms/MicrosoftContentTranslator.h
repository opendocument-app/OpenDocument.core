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
    MicrosoftContentTranslator();
    ~MicrosoftContentTranslator();

    void translate(const tinyxml2::XMLElement &in, TranslationContext &context) const;

private:
    class Impl;
    const std::unique_ptr<Impl> impl;
};

}

#endif //ODR_MICROSOFTCONTENTTRANSLATOR_H
