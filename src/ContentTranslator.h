#ifndef ODR_CONTENTTRANSLATOR_H
#define ODR_CONTENTTRANSLATOR_H

#include <iostream>
#include <memory>

namespace tinyxml2 {
class XMLElement;
}

namespace odr {

struct Context;

// TODO: we could have a "ContentTranslatorContext" containing (in, out, context) to reduce parameter count

class ContentTranslator {
public:
    static std::unique_ptr<ContentTranslator> create();

    virtual ~ContentTranslator() = default;
    virtual void translate(const tinyxml2::XMLElement &in, std::ostream &out, Context &context) const = 0;
};

}

#endif //ODR_CONTENTTRANSLATOR_H
