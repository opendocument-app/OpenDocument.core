#ifndef ODR_MICROSOFTTRANSLATOR_H
#define ODR_MICROSOFTTRANSLATOR_H

#include <string>
#include <memory>

namespace odr {

class MicrosoftOpenXmlFile;
class TranslationContext;

class MicrosoftTranslator {
public:
    static std::unique_ptr<MicrosoftTranslator> create();

    virtual ~MicrosoftTranslator() = default;
    virtual bool translate(MicrosoftOpenXmlFile &in, const std::string &out, TranslationContext &context) const = 0;
};

}

#endif //ODR_MICROSOFTTRANSLATOR_H
