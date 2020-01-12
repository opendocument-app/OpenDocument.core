#ifndef ODR_MICROSOFTTRANSLATOR_H
#define ODR_MICROSOFTTRANSLATOR_H

#include <string>
#include <memory>

namespace odr {

class TranslationContext;

class MicrosoftTranslator final {
public:
    MicrosoftTranslator();
    ~MicrosoftTranslator();

    bool translate(const std::string &outPath, TranslationContext &context) const;
    bool backTranslate(const std::string &diff, const std::string &outPath, TranslationContext &context) const;

private:
    class Impl;
    const std::unique_ptr<Impl> impl;
};

}

#endif //ODR_MICROSOFTTRANSLATOR_H
