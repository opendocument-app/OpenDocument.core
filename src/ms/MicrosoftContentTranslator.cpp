#include "MicrosoftContentTranslator.h"
#include "tinyxml2.h"
#include "glog/logging.h"
#include "odr/TranslationConfig.h"
#include "../TranslationContext.h"
#include "../xml/XmlTranslator.h"
#include "../xml/Xml2Html.h"
#include "MicrosoftOpenXmlFile.h"

namespace odr {

namespace {
class TableTranslator final : public DefaultElementTranslator {
public:
    TableTranslator() : DefaultElementTranslator("table") {
        addAttributes("border", "0");
        addAttributes("cellspacing", "0");
        addAttributes("cellpadding", "0");
    }
};

class TableCellTranslator final : public DefaultElementTranslator {
public:
    TableCellTranslator() : DefaultElementTranslator("td") {}

    void translateElementStart(const tinyxml2::XMLElement &in, TranslationContext &context) const final {
        const auto r = in.FindAttribute("r");
        // TODO check placement r and fill empty cells/rows

        DefaultElementTranslator::translateElementStart(in, context);
    }

    void translateElementChildren(const tinyxml2::XMLElement &in, TranslationContext &context) const final {
        const auto t = in.FindAttribute("t");
        if (t != nullptr) {
            if (std::strcmp(t->Value(), "s") == 0) {
                const auto sharedStringIndex = in.FirstChildElement("v")->IntText(-1);
                if (sharedStringIndex >= 0) {
                    const tinyxml2::XMLElement &replacement = *context.sharedStrings[sharedStringIndex];
                    DefaultElementTranslator::translateElementChildren(replacement, context);
                } else {
                    DLOG(INFO) << "undefined behaviour: shared string not found";
                }
            } else if ((std::strcmp(t->Value(), "inlineStr") == 0) ||
                    (std::strcmp(t->Value(), "n") == 0)) {
                DefaultElementTranslator::translateElementChildren(in, context);
            } else {
                DLOG(INFO) << "undefined behaviour: t=" << t->Value();
            }
        } else {
            DLOG(INFO) << "undefined behaviour: t not found";
        }
    }
};

class IgnoreHandler final : public XmlTranslator {
public:
    void translate(const tinyxml2::XMLElement &, TranslationContext &) const final {}
    void translate(const tinyxml2::XMLAttribute &, TranslationContext &) const final {}
    void translate(const tinyxml2::XMLText &, TranslationContext &) const final {}
};

class DefaultHandler final : public DefaultXmlTranslator {
public:
    void translate(const tinyxml2::XMLElement &in, TranslationContext &context) const final {
        translateElementChildren(in, context);
    }

    void translate(const tinyxml2::XMLAttribute &, TranslationContext &) const final {}
};
}

class MicrosoftContentTranslator::Impl final : public DefaultXmlTranslator {
public:
    DefaultElementTranslator paragraphTranslator;

    DefaultHandler defaultHandler;

    Impl() :
            paragraphTranslator("p") {
        addElementDelegation("w:p", paragraphTranslator.setDefaultDelegation(this));

        defaultHandler.setDefaultDelegation(this);
        setDefaultDelegation(&defaultHandler);
    }

    void translate(const tinyxml2::XMLElement &in, TranslationContext &context) const final {
        translateElementChild(in, context);
    }

    void translate(const tinyxml2::XMLText &in, TranslationContext &context) const final {
        *context.output << in.Value();
    }
};

MicrosoftContentTranslator::MicrosoftContentTranslator() :
        impl(std::make_unique<Impl>()) {
}

MicrosoftContentTranslator::~MicrosoftContentTranslator() = default;

void MicrosoftContentTranslator::translate(const tinyxml2::XMLElement &in, TranslationContext &context) const {
    impl->translate(in, context);
}

}
