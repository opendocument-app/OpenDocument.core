#ifndef OPENDOCUMENT_TEXTCONTENTTRANSLATOR_H
#define OPENDOCUMENT_TEXTCONTENTTRANSLATOR_H

#include "ContentTranslator.h"

namespace opendocument {

class TextContentTranslator : public ContentTranslator {
public:
    bool translate(tinyxml2::XMLElement &in, std::ostream &out) const override;
};

}

#endif //OPENDOCUMENT_TEXTCONTENTTRANSLATOR_H
