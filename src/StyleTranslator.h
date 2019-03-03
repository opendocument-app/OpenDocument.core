#ifndef OPENDOCUMENT_STYLETRANSLATOR_H
#define OPENDOCUMENT_STYLETRANSLATOR_H

#include <iostream>

namespace tinyxml2 {
class XMLElement;
}

namespace opendocument {

class StyleTranslator {
public:
    virtual ~StyleTranslator() = default;
    virtual bool translate(tinyxml2::XMLElement &in, std::ostream &out) const;
};

}

#endif //OPENDOCUMENT_STYLETRANSLATOR_H
