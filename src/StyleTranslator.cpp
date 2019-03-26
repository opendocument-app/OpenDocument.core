#include "StyleTranslator.h"
#include "odr/TranslationConfig.h"

namespace odr {

struct Context {
    TranslationConfig config;
};

class StyleElementTranslator {
public:
    virtual ~StyleElementTranslator() = default;
    virtual void translate(tinyxml2::XMLElement &in, std::ostream &out) const = 0;
};

class DefaultStyleTranslatorImpl : public StyleTranslator {
public:
    ~DefaultStyleTranslatorImpl() override = default;
    void translate(tinyxml2::XMLElement &in, std::ostream &out) const override {
    }
};

std::unique_ptr<StyleTranslator> StyleTranslator::create(const TranslationConfig &) {
    return std::make_unique<DefaultStyleTranslatorImpl>();
}

}
