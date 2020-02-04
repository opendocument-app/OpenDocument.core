#include "OfficeOpenXmlPresentationTranslator.h"
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

namespace {
void XfrmTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &) {
    const tinyxml2::XMLElement *offEle = in.FirstChildElement("a:off");
    if (offEle != nullptr) {
        float xIn = offEle->FindAttribute("x")->Int64Value() / 914400.0f;
        float yIn = offEle->FindAttribute("y")->Int64Value() / 914400.0f;
        out << "position:absolute;";
        out << "left:" << xIn << "in;";
        out << "top:" << yIn << "in;";
    }
    const tinyxml2::XMLElement *extEle = in.FirstChildElement("a:ext");
    if (extEle != nullptr) {
        float cxIn = extEle->FindAttribute("cx")->Int64Value() / 914400.0f;
        float cyIn = extEle->FindAttribute("cy")->Int64Value() / 914400.0f;
        out << "width:" << cxIn << "in;";
        out << "height:" << cyIn << "in;";
    }
}

void translateStyleInline(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context) {
    // <a:rPr b="1" lang="en-US" sz="4800" spc="-1" strike="noStrike">
    const tinyxml2::XMLAttribute *szAttr = in.FindAttribute("sz");
    if (szAttr != nullptr) {
        float szPt = szAttr->Int64Value() / 100.0;
        out << "font-size:" << szPt << "pt;";
    }

    XmlUtil::visitElementChildren(in, [&](const tinyxml2::XMLElement &e) {
        const std::string element = e.Name();

        if (element == "a:xfrm") XfrmTranslator(e, out, context);
    });
}
}

void OfficeOpenXmlPresentationTranslator::translateStyle(const tinyxml2::XMLElement &in, TranslationContext &context) {
}

namespace {
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

void StyleAttributeTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context) {
    const std::string prefix = in.Name();

    const tinyxml2::XMLElement *inlineStyle = in.FirstChildElement((prefix + "Pr").c_str());
    if (inlineStyle != nullptr && inlineStyle->FirstChild() != nullptr) {
        out << " style=\"";
        translateStyleInline(*inlineStyle, out, context);
        out << "\"";
    }
}

void ElementAttributeTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context) {
    StyleAttributeTranslator(in, out, context);
}

void ElementChildrenTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context);
void ElementTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context);

void SlideTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context) {
    out << "<div class=\"slide\">";
    ElementChildrenTranslator(in, out, context);
    out << "</div>";
}

void ShapeTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context) {
    out << "<div";
    ElementAttributeTranslator(in, out, context);
    out << ">";
    ElementChildrenTranslator(in, out, context);
    out << "</div>";
}

void ElementChildrenTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context) {
    XmlUtil::visitNodeChildren(in, [&](const tinyxml2::XMLNode &n) {
        if (n.ToText() != nullptr) TextTranslator(*n.ToText(), out, context);
        else if (n.ToElement() != nullptr) ElementTranslator(*n.ToElement(), out, context);
    });
}

void ElementTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context) {
    static std::unordered_map<std::string, const char *> substitution{
            {"a:p", "p"},
            {"a:r", "span"},
    };
    static std::unordered_set<std::string> skippers{
    };

    const std::string element = in.Name();
    if (skippers.find(element) != skippers.end()) return;

    if (element == "p:cSld") SlideTranslator(in, out, context);
    else if (element == "p:sp") ShapeTranslator(in, out, context);
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

void OfficeOpenXmlPresentationTranslator::translateContent(const tinyxml2::XMLElement &in, TranslationContext &context) {
    ElementTranslator(in, *context.output, context);
}

}
