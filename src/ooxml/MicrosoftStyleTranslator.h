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
    MicrosoftStyleTranslator();
    ~MicrosoftStyleTranslator();

    void translate(const tinyxml2::XMLElement &in, TranslationContext &context) const;

private:
    class Impl;
    const std::unique_ptr<Impl> impl;
};

}

#endif //ODR_MICROSOFTSTYLETRANSLATOR_H
