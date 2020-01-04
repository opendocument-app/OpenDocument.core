#include "MicrosoftStyleTranslator.h"
#include <string>
#include "tinyxml2.h"
#include "glog/logging.h"
#include "odr/TranslationConfig.h"
#include "../TranslationContext.h"
#include "../xml/XmlTranslator.h"

namespace odr {

namespace {
class FontTranslator final : public DefaultXmlTranslator {
public:
    void translateElementStart(const tinyxml2::XMLElement &in, TranslationContext &context) const final {
        const auto fontAttr = in.FindAttribute("w:cs");
        if (fontAttr == nullptr) return;
        *context.output << "font-family:" << fontAttr->Value() << ";";
    }
};

class FontSizeTranslator final : public DefaultXmlTranslator {
public:
    void translateElementStart(const tinyxml2::XMLElement &in, TranslationContext &context) const final {
        const auto sizeAttr = in.FindAttribute("w:val");
        if (sizeAttr == nullptr) return;
        const double size = std::stoi(sizeAttr->Value()) / 2.0;
        *context.output << "font-size:" << size << "pt;";
    }
};

class BoldTranslator final : public DefaultXmlTranslator {
public:
    void translateElementStart(const tinyxml2::XMLElement &in, TranslationContext &context) const final {
        const auto valAttr = in.FindAttribute("w:val");
        if (valAttr != nullptr) return;
        *context.output << "font-weight:bold;";
    }
};

class ItalicTranslator final : public DefaultXmlTranslator {
public:
    void translateElementStart(const tinyxml2::XMLElement &in, TranslationContext &context) const final {
        const auto valAttr = in.FindAttribute("w:val");
        if (valAttr != nullptr) return;
        *context.output << "font-style:italic;";
    }
};

class DefaultHandler final : public DefaultXmlTranslator {
public:
    FontTranslator fontTranslator;
    FontSizeTranslator fontSizeTranslator;
    BoldTranslator boldTranslator;
    ItalicTranslator italicTranslator;

    DefaultHandler() {
        addElementDelegation("w:rFonts", fontTranslator);
        addElementDelegation("w:sz", fontSizeTranslator);
        addElementDelegation("w:b", boldTranslator);
        addElementDelegation("w:i", italicTranslator);

        setDefaultDelegation(this);
    };

    void translate(const tinyxml2::XMLElement &in, TranslationContext &context) const final {
        translateElementChildren(in, context);
    }

    void translate(const tinyxml2::XMLAttribute &, TranslationContext &) const final {}
};

class DefaultStyleTranslator final : public DefaultXmlTranslator {
public:
    void translateElementStart(const tinyxml2::XMLElement &, TranslationContext &context) const final {
        *context.output << "* {";
    }

    void translateElementEnd(const tinyxml2::XMLElement &, TranslationContext &context) const final {
        *context.output << "}\n";
    }
};

class StyleClassTranslator final : public DefaultXmlTranslator {
public:
    DefaultHandler defaultHandler;

    StyleClassTranslator() {
        setDefaultDelegation(&defaultHandler);
    }

    void translateElementStart(const tinyxml2::XMLElement &in, TranslationContext &context) const final {
        const auto nameAttr = in.FindAttribute("w:styleId");
        std::string name = "unknown";
        if (nameAttr != nullptr) {
            name = nameAttr->Value();
        } else {
            LOG(WARNING) << "no name attribute " << in.Name();
        }

        const auto typeAttr = in.FindAttribute("w:type");
        std::string type = "unknown";
        if (typeAttr != nullptr) {
            type = typeAttr->Value();
        } else {
            LOG(WARNING) << "no type attribute " << in.Name();
        }

        /*
        const char *parentStyleName;
        if (in.QueryStringAttribute("style:parent-style-name", &parentStyleName) == tinyxml2::XML_SUCCESS) {
            context.odStyleDependencies[styleName].push_back(parentStyleName);
        }
         */

        *context.output << "." << name << " {";
    }

    void translateElementEnd(const tinyxml2::XMLElement &, TranslationContext &context) const final {
        *context.output << "}\n";
    }
};
}

class MicrosoftStyleTranslator::Impl final : public DefaultXmlTranslator {
public:
    DefaultStyleTranslator defaultStyleTranslator;
    StyleClassTranslator styleClassTranslator;

    Impl() {
        addElementDelegation("w:docDefaults", defaultStyleTranslator);
        addElementDelegation("w:style", styleClassTranslator);
    }
};

MicrosoftStyleTranslator::MicrosoftStyleTranslator() :
        impl(std::make_unique<Impl>()) {
}

MicrosoftStyleTranslator::~MicrosoftStyleTranslator() = default;

void MicrosoftStyleTranslator::translate(const tinyxml2::XMLElement &in, TranslationContext &context) const {
    impl->translate(in, context);
}

}
