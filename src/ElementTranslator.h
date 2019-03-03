#ifndef OPENDOCUMENT_ELEMENTTRANSLATOR_H
#define OPENDOCUMENT_ELEMENTTRANSLATOR_H

#include <iostream>

namespace tinyxml2 {
class XMLElement;
}

namespace opendocument {

class ElementTranslator {
public:
    virtual ~ElementTranslator() = default;
    virtual bool translate(tinyxml2::XMLElement &in, std::ostream &out) const;
};

}

#endif //OPENDOCUMENT_ELEMENTTRANSLATOR_H
