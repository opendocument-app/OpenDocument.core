#ifndef OPENDOCUMENT_CONTENTTRANSLATOR_H
#define OPENDOCUMENT_CONTENTTRANSLATOR_H

#include <iostream>

namespace tinyxml2 {
class XMLElement;
}

namespace opendocument {

class ContentTranslator {
public:
    virtual bool translate(tinyxml2::XMLElement &in, std::ostream &out) const;
};

}

#endif //OPENDOCUMENT_CONTENTTRANSLATOR_H
