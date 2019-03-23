#ifndef OPENDOCUMENT_CONTENTTRANSLATOR_H
#define OPENDOCUMENT_CONTENTTRANSLATOR_H

#include <iostream>
#include <memory>

namespace tinyxml2 {
class XMLElement;
}

namespace opendocument {

class ContentTranslator {
public:
    static std::unique_ptr<ContentTranslator> createDefaultContentTranslator();

    virtual ~ContentTranslator() = default;
    virtual bool translate(tinyxml2::XMLElement &in, std::ostream &out) const = 0;
};

}

#endif //OPENDOCUMENT_CONTENTTRANSLATOR_H
