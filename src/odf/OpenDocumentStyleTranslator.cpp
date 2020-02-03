#include "OpenDocumentStyleTranslator.h"
#include <string>
#include <unordered_map>
#include "tinyxml2.h"
#include "glog/logging.h"
#include "../TranslationContext.h"
#include "../StringUtil.h"
#include "../XmlUtil.h"

namespace odr {

namespace {
void StylePropertiesTranslator(const tinyxml2::XMLAttribute &in, std::ostream &out) {
    static std::unordered_map<std::string, const char *> substitution{
            {"fo:text-align", "text-align"},
            {"fo:font-size", "font-size"},
            {"fo:font-weight", "font-weight"},
            {"fo:font-style", "font-style"},
            {"fo:font-size", "font-size"},
            {"fo:text-shadow", "text-shadow"},
            {"fo:color", "color"},
            {"fo:background-color", "background-color"},
            {"fo:page-width", "width"},
            {"fo:page-height", "height"},
            {"fo:margin-top", "margin-top"},
            {"fo:margin-right", "margin-right"},
            {"fo:margin-bottom", "margin-bottom"},
            {"fo:margin-left", "margin-left"},
            {"fo:padding", "padding"},
            {"fo:padding-top", "padding-top"},
            {"fo:padding-right", "padding-right"},
            {"fo:padding-bottom", "padding-bottom"},
            {"fo:padding-left", "padding-left"},
            {"fo:border", "border"},
            {"fo:border-top", "border-top"},
            {"fo:border-right", "border-right"},
            {"fo:border-bottom", "border-bottom"},
            {"fo:border-left", "border-left"},
            {"style:font-name", "font-family"},
            {"style:width", "width"},
            {"style:height", "height"},
            {"style:vertical-align", "vertical-align"},
            {"style:column-width", "width"},
            {"style:row-height", "height"},
    };

    const std::string property = in.Name();
    const auto it = substitution.find(property);
    if (it != substitution.end()) {
        out << it->second << ":" << in.Value() << ";";
    } else if (property == "style:text-underline-style") {
        // TODO breaks line-through
        if (std::strcmp(in.Value(), "solid") == 0) out << "text-decoration:underline;";
    } else if (property == "style:text-line-through-style") {
        // TODO breaks underline
        if (std::strcmp(in.Value(), "solid") == 0) out << "text-decoration:line-through;";
    }
}

void StyleClassTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context) {
    static std::unordered_map<std::string, const char *> elementToNameAttr{
            {"style:default-style", "style:family"},
            {"style:style", "style:name"},
    };

    const std::string element = in.Name();
    const auto it = elementToNameAttr.find(element);
    if (it == elementToNameAttr.end()) return;

    const tinyxml2::XMLAttribute *nameAttr = in.FindAttribute(it->second);
    if (nameAttr == nullptr) {
        LOG(WARNING) << "skipped style " << in.Name() << ". no name attribute.";
        return;
    }
    const std::string name = OpenDocumentStyleTranslator::escapeStyleName(nameAttr->Value());

    const char *parentStyleName;
    if (in.QueryStringAttribute("style:parent-style-name", &parentStyleName) == tinyxml2::XML_SUCCESS) {
        context.odStyleDependencies[name].push_back(OpenDocumentStyleTranslator::escapeStyleName(parentStyleName));
    }
    const char *family;
    if (in.QueryStringAttribute("style:family", &family) == tinyxml2::XML_SUCCESS) {
        context.odStyleDependencies[name].push_back(OpenDocumentStyleTranslator::escapeStyleName(family));
    }

    out << "." << name << " {";

    XmlUtil::visitElementChildren(in, [&](const tinyxml2::XMLElement &e) {
        XmlUtil::visitElementAttributes(e, [&](const tinyxml2::XMLAttribute &a) {
            StylePropertiesTranslator(a, out);
        });
    });

    out << "}\n";
}

void ListStyleTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context) {
    //addElementDelegation("text:list-level-style-number", propertiesTranslator);
    //addElementDelegation("text:list-level-style-bullet", propertiesTranslator);

    const auto styleNameAttr = in.Parent()->ToElement()->FindAttribute("style:name");
    if (styleNameAttr == nullptr) {
        LOG(WARNING) << "skipped style " << in.Parent()->ToElement()->Name() << ". no name attribute.";
        return;
    }
    const std::string styleName = OpenDocumentStyleTranslator::escapeStyleName(styleNameAttr->Value());
    context.odStyleDependencies[styleName] = {};

    const auto listLevelAttr = in.FindAttribute("text:level");
    if (listLevelAttr == nullptr) {
        LOG(WARNING) << "cannot find level attribute";
        return;
    }
    const std::uint32_t listLevel = listLevelAttr->UnsignedValue();

    std::string selector = "ul." + styleName;
    for (std::uint32_t i = 1; i < listLevel; ++i) {
        selector += " li";
    }

    const auto bulletCharAttr = in.FindAttribute("text:bullet-char");
    const auto numFormatAttr = in.FindAttribute("text:num-format");
    if (bulletCharAttr != nullptr) {
        *context.output << selector << " {";
        *context.output << "list-style: none;";
        *context.output << "}\n";
        *context.output << selector << " li:before {";
        *context.output << "content: \"" << bulletCharAttr->Value() << "\";";
        *context.output << "}\n";
    } else if (numFormatAttr != nullptr) {
        // TODO check attribute value and switch
        *context.output << selector << " {";
        *context.output << "list-style: decimal;";
        *context.output << "}\n";
    } else {
        LOG(WARNING) << "unhandled case";
    }
}
}

std::string OpenDocumentStyleTranslator::escapeStyleName(const std::string &name) {
    std::string result = name;
    StringUtil::findAndReplaceAll(result, ".", "_");
    return result;
}

void OpenDocumentStyleTranslator::translate(const tinyxml2::XMLElement &in, TranslationContext &context) {
    XmlUtil::visitElementChildren(in, [&](const tinyxml2::XMLElement &e) {
        StyleClassTranslator(e, *context.output, context);
        // TODO ListStyleTranslator
    });
}

}
