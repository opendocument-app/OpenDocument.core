#include "MicrosoftContentTranslator.h"
#include "tinyxml2.h"
#include "glog/logging.h"
#include "odr/TranslationConfig.h"
#include "../TranslationContext.h"
#include "../xml/XmlTranslator.h"
#include "../xml/Xml2Html.h"

namespace odr {

namespace {
class ParagraphTranslator final : public DefaultElementTranslator {
public:
    ParagraphTranslator() : DefaultElementTranslator("p") {}

    void translateElementAttributes(const tinyxml2::XMLElement &in, TranslationContext &context) const final {
        const tinyxml2::XMLElement *style = tinyxml2::XMLHandle((tinyxml2::XMLElement &) in)
                .FirstChildElement("w:pPr")
                .FirstChildElement("w:pStyle")
                .ToElement();
        if (style != nullptr) {
            *context.output << " class=\"" << style->FindAttribute("w:val")->Value() <<  "\"";
        }

        DefaultElementTranslator::translateElementAttributes(in, context);
    }
};

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
        //const auto r = in.FindAttribute("r");
        // TODO check placement r and fill empty cells/rows

        DefaultElementTranslator::translateElementStart(in, context);
    }

    void translateElementChildren(const tinyxml2::XMLElement &in, TranslationContext &context) const final {
        const auto t = in.FindAttribute("t");
        if (t != nullptr) {
            if (std::strcmp(t->Value(), "s") == 0) {
                const auto sharedStringIndex = in.FirstChildElement("v")->IntText(-1);
                if (sharedStringIndex >= 0) {
                    const tinyxml2::XMLElement &replacement = *context.msSharedStrings[sharedStringIndex];
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
    ParagraphTranslator paragraphTranslator;

    DefaultElementTranslator slidTranslator;
    DefaultElementTranslator boxTranslator;
    DefaultElementTranslator boxParagraphTranslator;

    TableTranslator tableTranslator;
    DefaultElementTranslator tableRowTranslator;
    TableCellTranslator tableCellTranslator;

    IgnoreHandler skipper;
    DefaultHandler defaultHandler;

    Impl() :
            slidTranslator("div"),
            boxTranslator("div"),
            boxParagraphTranslator("p"),
            tableRowTranslator("tr") {
        // document
        addElementDelegation("w:p", paragraphTranslator.setDefaultDelegation(this));

        // presentation
        addElementDelegation("p:cSld", slidTranslator.setDefaultDelegation(this));
        addElementDelegation("p:sp", boxTranslator.setDefaultDelegation(this));
        addElementDelegation("a:p", boxParagraphTranslator.setDefaultDelegation(this));

        // workbook
        addElementDelegation("worksheet", tableTranslator.setDefaultDelegation(this));
        addElementDelegation("row", tableRowTranslator.setDefaultDelegation(this));
        addElementDelegation("c", tableCellTranslator.setDefaultDelegation(this));
        addElementDelegation("headerFooter", skipper);

        defaultHandler.setDefaultDelegation(this);
        setDefaultDelegation(&defaultHandler);
    }

    void translate(const tinyxml2::XMLElement &in, TranslationContext &context) const final {
        translateElementChild(in, context);
    }

    void translate(const tinyxml2::XMLText &in, TranslationContext &context) const final {
        if (!context.config->editable) {
            *context.output << in.Value();
        } else {
            *context.output << "<span contenteditable=\"true\" data-odr-cid=\""
                    << context.currentTextTranslationIndex << "\">" << in.Value() << "</span>";
            context.textTranslation[context.currentTextTranslationIndex] = &in;
            ++context.currentTextTranslationIndex;
        }
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
