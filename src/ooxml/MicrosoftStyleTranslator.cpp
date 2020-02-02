#include "MicrosoftStyleTranslator.h"
#include <string>
#include "tinyxml2.h"
#include "glog/logging.h"
#include "odr/TranslationConfig.h"
#include "../TranslationContext.h"
#include "../XmlUtil.h"

namespace odr {

static void AlignmentTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &) {
    const auto valAttr = in.FindAttribute("w:val");
    out << "text-align:" << valAttr->Value() << ";";
}

static void FontTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &) {
    const auto fontAttr = in.FindAttribute("w:cs");
    if (fontAttr == nullptr) return;
    out << "font-family:" << fontAttr->Value() << ";";
}

static void FontSizeTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &) {
    const auto sizeAttr = in.FindAttribute("w:val");
    if (sizeAttr == nullptr) return;
    const double size = std::stoi(sizeAttr->Value()) / 2.0;
    out << "font-size:" << size << "pt;";
}

static void BoldTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &) {
    const auto valAttr = in.FindAttribute("w:val");
    if (valAttr != nullptr) return;
    out << "font-weight:bold;";
}

static void ItalicTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &) {
    const auto valAttr = in.FindAttribute("w:val");
    if (valAttr != nullptr) return;
    out << "font-style:italic;";
}

static void UnderlineTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &) {
    const auto valAttr = in.FindAttribute("w:val");
    if (std::strcmp(valAttr->Value(), "single") == 0) out << "text-decoration:underline;";
    // TODO wont work with StrikeThroughTranslator
}

static void StrikeThroughTranslator(const tinyxml2::XMLElement &, std::ostream &out, TranslationContext &) {
    // TODO wont work with UnderlineTranslator
    out << "text-decoration:line-through;";
}

static void ShadowTranslator(const tinyxml2::XMLElement &, std::ostream &out, TranslationContext &) {
    out << "text-shadow:1pt 1pt;";
}

static void ColorTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &) {
    const auto valAttr = in.FindAttribute("w:val");
    if (std::strcmp(valAttr->Value(), "auto") == 0) return;
    if (std::strlen(valAttr->Value()) == 6) out << "color:#" << valAttr->Value();
    else out << "color:" << valAttr->Value();
}

static void HighlightTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &) {
    const auto valAttr = in.FindAttribute("w:val");
    if (std::strcmp(valAttr->Value(), "auto") == 0) return;
    if (std::strlen(valAttr->Value()) == 6) out << "background-color:#" << valAttr->Value();
    else out << "background-color:" << valAttr->Value();
}

static void IndentationTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &) {
    const tinyxml2::XMLAttribute *attrLeft = in.FindAttribute("w:left");
    if (attrLeft != nullptr) out << "margin-left:" << attrLeft->Int64Value() / 1440.0f << "in;";
    const tinyxml2::XMLAttribute *attrRight = in.FindAttribute("w:right");
    if (attrRight != nullptr) out << "margin-right:" << attrRight->Int64Value() / 1440.0f << "in;";
}

static void StyleClassTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context) {
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

    out << "." << name << " {";

    const auto pPr = in.FirstChildElement("w:pPr");
    if (pPr != nullptr) MicrosoftStyleTranslator::translateInline(*pPr, out, context);

    const auto rPr = in.FirstChildElement("w:rPr");
    if (rPr != nullptr) MicrosoftStyleTranslator::translateInline(*rPr, out, context);

    out << "}\n";
}

void MicrosoftStyleTranslator::translate(const tinyxml2::XMLElement &in, TranslationContext &context) {
    XmlUtil::visitElementChildren(in, [&](const tinyxml2::XMLElement &e) {
        const std::string element = e.Name();

        if (element == "w:style") StyleClassTranslator(e, *context.output, context);
        //else if (element == "w:docDefaults") DefaultStyleTranslator(e, *context.output, context);
    });
}

void MicrosoftStyleTranslator::translateInline(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context) {
    XmlUtil::visitElementChildren(in, [&](const tinyxml2::XMLElement &e) {
        const std::string element = e.Name();

        if (element == "w:jc") AlignmentTranslator(e, out, context);
        else if (element == "w:rFonts") FontTranslator(e, out, context);
        else if (element == "w:sz") FontSizeTranslator(e, out, context);
        else if (element == "w:b") BoldTranslator(e, out, context);
        else if (element == "w:i") ItalicTranslator(e, out, context);
        else if (element == "w:u") UnderlineTranslator(e, out, context);
        else if (element == "w:strike") StrikeThroughTranslator(e, out, context);
        else if (element == "w:shadow") ShadowTranslator(e, out, context);
        else if (element == "w:color") ColorTranslator(e, out, context);
        else if (element == "w:highlight") HighlightTranslator(e, out, context);
        else if (element == "w:ind") IndentationTranslator(e, out, context);
    });
}

}
