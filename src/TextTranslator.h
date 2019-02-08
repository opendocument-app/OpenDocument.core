#ifndef OPENDOCUMENT_TEXTTRANSLATOR_H
#define OPENDOCUMENT_TEXTTRANSLATOR_H

#include <string>

namespace opendocument {

class OpenDocumentFile;

class TextTranslator final {
public:
    TextTranslator();
    ~TextTranslator();

    bool translate(OpenDocumentFile &in, const std::string &out) const;

private:
};

}

#endif //OPENDOCUMENT_TEXTTRANSLATOR_H
