#ifndef ODR_MICROSOFTSTYLETRANSLATOR_H
#define ODR_MICROSOFTSTYLETRANSLATOR_H

#include <memory>
#include <ostream>

namespace tinyxml2 {
class XMLElement;
}

namespace odr {

struct TranslationContext;

class MicrosoftStyleTranslator final {
public:
    static void translate(const tinyxml2::XMLElement &in, TranslationContext &context);
    static void translateInline(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context);
};

}

#endif //ODR_MICROSOFTSTYLETRANSLATOR_H
