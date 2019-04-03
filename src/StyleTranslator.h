#ifndef ODR_STYLETRANSLATOR_H
#define ODR_STYLETRANSLATOR_H

#include <iostream>
#include <memory>

namespace tinyxml2 {
class XMLElement;
}

namespace odr {

struct Context;

class StyleTranslator {
public:
    static std::unique_ptr<StyleTranslator> create();

    virtual ~StyleTranslator() = default;
    virtual void translate(const tinyxml2::XMLElement &in, std::ostream &out, Context &context) const = 0;
};

}

#endif //ODR_STYLETRANSLATOR_H
