#ifndef ODR_CONTENTTRANSLATOR_H
#define ODR_CONTENTTRANSLATOR_H

#include <iostream>
#include <memory>

namespace tinyxml2 {
class XMLElement;
}

namespace odr {

struct TranslationConfig;

class ContentTranslator {
public:
    static std::unique_ptr<ContentTranslator> create(const TranslationConfig &);

    virtual ~ContentTranslator() = default;
    virtual void translate(tinyxml2::XMLElement &in, std::ostream &out) const = 0;
};

}

#endif //ODR_CONTENTTRANSLATOR_H
