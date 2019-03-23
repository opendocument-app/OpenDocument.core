#ifndef OPENDOCUMENT_STYLEELEMENTTRANSLATOR_H
#define OPENDOCUMENT_STYLEELEMENTTRANSLATOR_H

#include <iostream>

namespace tinyxml2 {
class XMLElement;
}

namespace opendocument {

class StyleElementTranslator {
public:
    virtual ~StyleElementTranslator() = default;
    virtual bool translate(tinyxml2::XMLElement &in, std::ostream &out) const;
};

}

#endif //OPENDOCUMENT_STYLEELEMENTTRANSLATOR_H
