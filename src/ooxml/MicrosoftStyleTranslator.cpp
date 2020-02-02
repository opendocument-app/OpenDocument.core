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
    const tinyxml2::XMLAttribute *leftAttr = in.FindAttribute("w:left");
    if (leftAttr != nullptr) out << "margin-left:" << leftAttr->Int64Value() / 1440.0f << "in;";
    const tinyxml2::XMLAttribute *rightAttr = in.FindAttribute("w:right");
    if (rightAttr != nullptr) out << "margin-right:" << rightAttr->Int64Value() / 1440.0f << "in;";
}

static void TableCellWidthTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &) {
    const tinyxml2::XMLAttribute *widthAttr = in.FindAttribute("w:w");
    const tinyxml2::XMLAttribute *typeAttr = in.FindAttribute("w:type");
    if (widthAttr != nullptr) {
        float width = widthAttr->Int64Value();
        if (typeAttr != nullptr && std::strcmp(typeAttr->Value(), "dxa") == 0) width /= 1440.0f;
        out << "width:" << width << "in;";
    }
}

static void TableCellBorderTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &) {
    auto translator = [&](const char *name, const tinyxml2::XMLElement &e) {
        out << name << ":";

        const float sizePt = e.FindAttribute("w:sz")->IntValue() / 2.0f;
        out << sizePt << "pt ";

        const char *type = "solid";
        if (std::strcmp(e.FindAttribute("w:val")->Value(), "") == 0) type = "solid";
        out << type << " ";

        const auto colorAttr = e.FindAttribute("w:color");
        if (std::strlen(colorAttr->Value()) == 6) out << "#" << colorAttr->Value();
        else out << colorAttr->Value();

        out << ";";
    };

    const tinyxml2::XMLElement *top = in.FirstChildElement("w:top");
    if (top != nullptr) translator("border-top", *top);

    const tinyxml2::XMLElement *left = in.FirstChildElement("w:left");
    if (left != nullptr) translator("border-left", *left);

    const tinyxml2::XMLElement *bottom = in.FirstChildElement("w:bottom");
    if (bottom != nullptr) translator("border-bottom", *bottom);

    const tinyxml2::XMLElement *right = in.FirstChildElement("w:right");
    if (right != nullptr) translator("border-right", *right);
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
        else if (element == "w:tcW") TableCellWidthTranslator(e, out, context);
        else if (element == "w:tcBorders") TableCellBorderTranslator(e, out, context);
    });
}

}
