#ifndef OPENDOCUMENT_CONTENTTRANSLATOR_H
#define OPENDOCUMENT_CONTENTTRANSLATOR_H

#include <iostream>

namespace tinyxml2 {
class XMLElement;
}

namespace opendocument {

class ContentTranslator {
public:
    virtual ~ContentTranslator() = default;
    virtual bool translate(tinyxml2::XMLElement &in, std::ostream &out) const;
};

class TextContentTranslator : public ContentTranslator {
public:
    ~TextContentTranslator() override = default;
    bool translate(tinyxml2::XMLElement &in, std::ostream &out) const override;
};

}

#endif //OPENDOCUMENT_CONTENTTRANSLATOR_H
