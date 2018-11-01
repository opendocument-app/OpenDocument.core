#ifndef OPENDOCUMENT_TEXTTRANSLATOR_H
#define OPENDOCUMENT_TEXTTRANSLATOR_H

#include "Translator.h"

namespace opendocument {

class TextTranslator : public Translator {
public:
    struct Config : public Translator::Config {
    };

    TextTranslator(const Config &config);
    ~TextTranslator();
private:
    const Config _config;
};

}

#endif //OPENDOCUMENT_TEXTTRANSLATOR_H
