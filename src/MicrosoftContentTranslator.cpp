#include "MicrosoftContentTranslator.h"
#include <utility>
#include <string>
#include <list>
#include <unordered_map>
#include "tinyxml2.h"
#include "glog/logging.h"
#include "odr/TranslationConfig.h"
#include "MicrosoftContext.h"
#include "MicrosoftOpenXmlFile.h"
#include "CryptoUtil.h"

namespace odr {

namespace {

class ElementTranslator {
public:
    virtual ~ElementTranslator() = default;
    virtual void translateStart(const tinyxml2::XMLElement &in, std::ostream &out, MicrosoftContext &context) const = 0;
    virtual void translateEnd(const tinyxml2::XMLElement &in, std::ostream &out, MicrosoftContext &context) const = 0;
};

class DefaultElementTranslator : public ElementTranslator {
public:
    const std::string name;

    explicit DefaultElementTranslator(const std::string &name) : name(name) {
    }

    ~DefaultElementTranslator() override = default;

    void translateStart(const tinyxml2::XMLElement &in, std::ostream &out, MicrosoftContext &context) const override {
        out << "<" << name;
        translateStartCallback(in, out, context);
        out << ">";
    }

    void translateEnd(const tinyxml2::XMLElement &in, std::ostream &out, MicrosoftContext &context) const override {
        out << "</" << name << ">";
    }

protected:
    virtual void translateStartCallback(const tinyxml2::XMLElement &in, std::ostream &out, MicrosoftContext &context) const {}
};

class ParagraphTranslator : public DefaultElementTranslator {
public:
    ParagraphTranslator() : DefaultElementTranslator("p") {}
    ~ParagraphTranslator() override = default;
};

class DefaultContentTranslator : public MicrosoftContentTranslator {
public:
    std::unordered_map<std::string, std::unique_ptr<ElementTranslator>> elementTranslator;

    DefaultContentTranslator() {
        elementTranslator["w:body"] = nullptr;
        elementTranslator["w:p"] = std::make_unique<ParagraphTranslator>();
        elementTranslator["w:r"] = nullptr;
    }

    ~DefaultContentTranslator() override = default;

    void translate(const tinyxml2::XMLElement &in, MicrosoftContext &context) const override {
        auto &out = *context.output;
        context.currentElement = &in;

        const std::string elementName = in.Name();
        auto elementTranslatorIt = elementTranslator.find(elementName);
        const ElementTranslator *translator = nullptr;
        if (elementTranslatorIt != elementTranslator.end()) {
            translator = elementTranslatorIt->second.get();
        } else {
            LOG(WARNING) << "unhandled element: " << elementName;
        }

        if (translator != nullptr) {
            translator->translateStart(in, out, context);
        }

        for (auto child = in.FirstChild(); child != nullptr; child = child->NextSibling()) {
            if (child->ToText() != nullptr) {
                out << child->ToText()->Value();
            } else if (child->ToElement() != nullptr) {
                translate(*child->ToElement(), context);
            }
        }

        if (translator != nullptr) {
            translator->translateEnd(in, out, context);
        }
    }
};

}

std::unique_ptr<MicrosoftContentTranslator> MicrosoftContentTranslator::create() {
    return std::make_unique<DefaultContentTranslator>();
}

}
