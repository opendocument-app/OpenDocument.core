#ifndef ODR_MICROSOFTSTYLETRANSLATOR_H
#define ODR_MICROSOFTSTYLETRANSLATOR_H

#include <memory>

namespace tinyxml2 {
class XMLElement;
}

namespace odr {

struct TranslationContext;

class MicrosoftStyleTranslator final {
public:
    static void translate(const tinyxml2::XMLElement &in, TranslationContext &context);
};

}

#endif //ODR_MICROSOFTSTYLETRANSLATOR_H
