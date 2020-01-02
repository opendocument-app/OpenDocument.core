#ifndef ODR_MICROSOFTTRANSLATOR_H
#define ODR_MICROSOFTTRANSLATOR_H

#include <string>
#include <memory>

namespace odr {

class MicrosoftOpenXmlFile;
class TranslationContext;

class MicrosoftTranslator final {
public:
    MicrosoftTranslator();
    ~MicrosoftTranslator();

    bool translate(MicrosoftOpenXmlFile &in, const std::string &out, TranslationContext &context) const;
    bool backTranslate(MicrosoftOpenXmlFile &in, const std::string &diff, const std::string &out, TranslationContext &context) const;

private:
    class Impl;
    const std::unique_ptr<Impl> impl;
};

}

#endif //ODR_MICROSOFTTRANSLATOR_H
