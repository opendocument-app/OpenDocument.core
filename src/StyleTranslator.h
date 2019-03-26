#ifndef ODR_STYLETRANSLATOR_H
#define ODR_STYLETRANSLATOR_H

#include <iostream>
#include <memory>

namespace tinyxml2 {
class XMLElement;
}

namespace odr {

struct TranslationConfig;

class StyleTranslator {
public:
    static std::unique_ptr<StyleTranslator> create(const TranslationConfig &);

    virtual ~StyleTranslator() = default;
    virtual void translate(tinyxml2::XMLElement &in, std::ostream &out) const = 0;
};

}

#endif //ODR_STYLETRANSLATOR_H
