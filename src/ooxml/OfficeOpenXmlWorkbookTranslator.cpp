#include "OfficeOpenXmlWorkbookTranslator.h"
#include <string>
#include <unordered_set>
#include <unordered_map>
#include "tinyxml2.h"
#include "glog/logging.h"
#include "odr/TranslationConfig.h"
#include "../TranslationContext.h"
#include "../StringUtil.h"
#include "../io/Storage.h"
#include "../io/StreamUtil.h"
#include "../XmlUtil.h"

namespace odr {

void OfficeOpenXmlWorkbookTranslator::translateStyle(const tinyxml2::XMLElement &in, TranslationContext &context) {
}

namespace {
void TextTranslator(const tinyxml2::XMLText &in, std::ostream &out, TranslationContext &context);
void AttributeTranslator(const tinyxml2::XMLAttribute &in, std::ostream &out, TranslationContext &context);
void ElementAttributeTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context);
void ElementChildrenTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context);
void ElementTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context);

void TableTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context) {
    out << R"(<table border="0" cellspacing="0" cellpadding="0")";
    ElementAttributeTranslator(in, out, context);
    out << ">";
    ElementChildrenTranslator(in, out, context);
    out << "</table>";
}

void TableCellTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context) {
    //const auto r = in.FindAttribute("r");
    // TODO check placement r and fill empty cells/rows

    out << "<td";
    ElementAttributeTranslator(in, out, context);
    out << ">";

    const auto t = in.FindAttribute("t");
    if (t != nullptr) {
        if (std::strcmp(t->Value(), "s") == 0) {
            const auto sharedStringIndex = in.FirstChildElement("v")->IntText(-1);
            if (sharedStringIndex >= 0) {
                const tinyxml2::XMLElement &replacement = *context.msSharedStrings[sharedStringIndex];
                ElementChildrenTranslator(replacement, out, context);
            } else {
                DLOG(INFO) << "undefined behaviour: shared string not found";
            }
        } else if ((std::strcmp(t->Value(), "inlineStr") == 0) ||
                   (std::strcmp(t->Value(), "n") == 0)) {
            ElementChildrenTranslator(in, out, context);
        } else {
            DLOG(INFO) << "undefined behaviour: t=" << t->Value();
        }
    } else {
        DLOG(INFO) << "undefined behaviour: t not found";
    }

    out << "</td>";
}

void TextTranslator(const tinyxml2::XMLText &in, std::ostream &out, TranslationContext &context) {
    std::string text = in.Value();
    StringUtil::findAndReplaceAll(text, "&", "&amp;");
    StringUtil::findAndReplaceAll(text, "<", "&lt;");
    StringUtil::findAndReplaceAll(text, ">", "&gt;");

    if (!context.config->editable) {
        out << text;
    } else {
        out << R"(<span contenteditable="true" data-odr-cid=")"
            << context.currentTextTranslationIndex << "\">" << text << "</span>";
        context.textTranslation[context.currentTextTranslationIndex] = &in;
        ++context.currentTextTranslationIndex;
    }
}

void AttributeTranslator(const tinyxml2::XMLAttribute &, std::ostream &, TranslationContext &) {
}

void ElementAttributeTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context) {
    XmlUtil::visitElementAttributes(in, [&](const tinyxml2::XMLAttribute &a) {
        AttributeTranslator(a, out, context);
    });
}

void ElementChildrenTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context) {
    XmlUtil::visitNodeChildren(in, [&](const tinyxml2::XMLNode &n) {
        if (n.ToText() != nullptr) TextTranslator(*n.ToText(), out, context);
        else if (n.ToElement() != nullptr) ElementTranslator(*n.ToElement(), out, context);
    });
}

void ElementTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context) {
    static std::unordered_map<std::string, const char *> substitution{
            {"row", "tr"},
    };
    static std::unordered_set<std::string> skippers{
            "headerFooter",
    };

    const std::string element = in.Name();
    if (skippers.find(element) != skippers.end()) return;

    if (element == "worksheet") TableTranslator(in, out, context);
    else if (element == "c") TableCellTranslator(in, out, context);
    else {
        const auto it = substitution.find(element);
        if (it != substitution.end()) {
            out << "<" << it->second;
            ElementAttributeTranslator(in, out, context);
            out << ">";
        }
        ElementChildrenTranslator(in, out, context);
        if (it != substitution.end()) {
            out << "</" << it->second << ">";
        }
    }
}
}

void OfficeOpenXmlWorkbookTranslator::translateContent(const tinyxml2::XMLElement &in, TranslationContext &context) {
    ElementTranslator(in, *context.output, context);
}

}
