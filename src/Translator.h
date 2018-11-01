#ifndef OPENDOCUMENT_TRANSLATOR_H
#define OPENDOCUMENT_TRANSLATOR_H

#include <string>
#include "OpenDocumentFile.h"

namespace opendocument {

class Translator {
public:
    struct Config {
        std::string tmp;
    };

    Translator(const Config &config);
    virtual ~Translator();

    virtual void translate(const OpenDocumentFile &file) = 0;
private:
};

}

#endif //OPENDOCUMENT_TRANSLATOR_H
