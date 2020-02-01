#include "MicrosoftStyleTranslator.h"
#include <string>
#include "tinyxml2.h"
#include "glog/logging.h"
#include "odr/TranslationConfig.h"
#include "../TranslationContext.h"
#include "../XmlUtil.h"

namespace odr {

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

    XmlUtil::recursiveVisitElements(&in, [&](const tinyxml2::XMLElement &e) {
        const std::string element = e.Name();

        if (element == "w:rFonts") FontTranslator(e, out, context);
        else if (element == "w:sz") FontSizeTranslator(e, out, context);
        else if (element == "w:b") BoldTranslator(e, out, context);
        else if (element == "w:i") ItalicTranslator(e, out, context);
    });

    out << "}\n";
}

void MicrosoftStyleTranslator::translate(const tinyxml2::XMLElement &in, TranslationContext &context) {
    XmlUtil::visitElementChildren(in, [&](const tinyxml2::XMLElement &e) {
        const std::string element = e.Name();

        if (element == "w:style") StyleClassTranslator(e, *context.output, context);
        //else if (element == "w:docDefaults") DefaultStyleTranslator(e, *context.output, context);
    });
}

}
