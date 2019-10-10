#include "OpenDocumentStyleTranslator.h"
#include <string>
#include <unordered_map>
#include "tinyxml2.h"
#include "glog/logging.h"
#include "odr/TranslationConfig.h"
#include "../TranslationContext.h"
#include "../StringUtil.h"
#include "../xml/XmlTranslator.h"

namespace odr {

namespace {
class StylePropertiesTranslator final : public DefaultXmlTranslator {
public:
    std::unordered_map<std::string, const char *> attributeTranslator;

    StylePropertiesTranslator() {
        attributeTranslator["fo:text-align"] = "text-align";
        attributeTranslator["fo:font-size"] = "font-size";
        attributeTranslator["fo:font-weight"] = "font-weight";
        attributeTranslator["fo:font-style"] = "font-style";
        attributeTranslator["fo:font-size"] = "font-size";
        attributeTranslator["fo:text-shadow"] = "text-shadow";
        attributeTranslator["fo:color"] = "color";
        attributeTranslator["fo:background-color"] = "background-color";
        attributeTranslator["fo:page-width"] = "width";
        attributeTranslator["fo:page-height"] = "height";
        attributeTranslator["fo:margin-top"] = "margin-top";
        attributeTranslator["fo:margin-right"] = "margin-right";
        attributeTranslator["fo:margin-bottom"] = "margin-bottom";
        attributeTranslator["fo:margin-left"] = "margin-left";
        attributeTranslator["fo:padding"] = "padding";
        attributeTranslator["fo:padding-top"] = "padding-top";
        attributeTranslator["fo:padding-right"] = "padding-right";
        attributeTranslator["fo:padding-bottom"] = "padding-bottom";
        attributeTranslator["fo:padding-left"] = "padding-left";
        attributeTranslator["fo:border"] = "border";
        attributeTranslator["fo:border-top"] = "border-top";
        attributeTranslator["fo:border-right"] = "border-right";
        attributeTranslator["fo:border-bottom"] = "border-bottom";
        attributeTranslator["fo:border-left"] = "border-left";

        attributeTranslator["style:font-name"] = "font-family";
        attributeTranslator["style:width"] = "width";
        attributeTranslator["style:height"] = "height";
        attributeTranslator["style:vertical-align"] = "vertical-align";
        attributeTranslator["style:column-width"] = "width";
        attributeTranslator["style:row-height"] = "height";
    }

    void translate(const tinyxml2::XMLAttribute &in, TranslationContext &context) const final {
        const std::string attributeName = in.Name();
        const auto it = attributeTranslator.find(attributeName);
        if (it != attributeTranslator.end()) {
            *context.output << it->second << ":" << in.Value() << ";";
        }
    }
};

class StyleClassTranslator final : public DefaultXmlTranslator {
public:
    const std::string nameAttribute;
    StylePropertiesTranslator propertiesTranslator;

    explicit StyleClassTranslator(const std::string &nameAttribute)
            : nameAttribute(nameAttribute) {
        addElementDelegation("style:text-properties", propertiesTranslator);
        addElementDelegation("style:paragraph-properties", propertiesTranslator);
        addElementDelegation("style:graphic-properties", propertiesTranslator);
        addElementDelegation("style:table-properties", propertiesTranslator);
        addElementDelegation("style:table-column-properties", propertiesTranslator);
        addElementDelegation("style:table-row-properties", propertiesTranslator);
        addElementDelegation("style:table-cell-properties", propertiesTranslator);
        addElementDelegation("style:page-layout-properties", propertiesTranslator);
        addElementDelegation("style:section-properties", propertiesTranslator);
        addElementDelegation("style:drawing-page-properties", propertiesTranslator);
    }

    void translateElementStart(const tinyxml2::XMLElement &in, TranslationContext &context) const final {
        const auto styleNameAttr = in.FindAttribute(nameAttribute.c_str());
        if (styleNameAttr == nullptr) {
            LOG(WARNING) << "skipped style " << in.Name() << ". no name attribute.";
            return;
        }
        const std::string styleName = OpenDocumentStyleTranslator::escapeStyleName(styleNameAttr->Value());

        const char *parentStyleName;
        if (in.QueryStringAttribute("style:parent-style-name", &parentStyleName) == tinyxml2::XML_SUCCESS) {
            context.odStyleDependencies[styleName].push_back(OpenDocumentStyleTranslator::escapeStyleName(parentStyleName));
        }
        const char *family;
        if (in.QueryStringAttribute("style:family", &family) == tinyxml2::XML_SUCCESS) {
            context.odStyleDependencies[styleName].push_back(OpenDocumentStyleTranslator::escapeStyleName(family));
        }

        *context.output << "." << styleName << " {";
    }

    void translateElementEnd(const tinyxml2::XMLElement &, TranslationContext &context) const final {
        *context.output << "}\n";
    }
};

class ListStyleTranslator final : public DefaultXmlTranslator {
public:
    StylePropertiesTranslator propertiesTranslator;

    explicit ListStyleTranslator() {
        addElementDelegation("text:list-level-style-number", propertiesTranslator);
        addElementDelegation("text:list-level-style-bullet", propertiesTranslator);
    }

    void translateElementChild(const tinyxml2::XMLElement &in, TranslationContext &context) const final {
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
};
}

std::string OpenDocumentStyleTranslator::escapeStyleName(const std::string &name) {
    std::string result = name;
    StringUtil::findAndReplaceAll(result, ".", "_");
    return result;
}

class OpenDocumentStyleTranslator::Impl final : public DefaultXmlTranslator {
public:
    StyleClassTranslator familyStyleClassTranslator;
    StyleClassTranslator nameStyleClassTranslator;
    ListStyleTranslator listStyleTranslator;

    Impl() :
            familyStyleClassTranslator("style:family"),
            nameStyleClassTranslator("style:name") {
        addElementDelegation("style:default-style", familyStyleClassTranslator);
        addElementDelegation("style:style", nameStyleClassTranslator);
        // TODO
        //addElementDelegation("text:list-style", listStyleTranslator);
    }
};

OpenDocumentStyleTranslator::OpenDocumentStyleTranslator() :
        impl(std::make_unique<Impl>()) {
}

OpenDocumentStyleTranslator::~OpenDocumentStyleTranslator() = default;

void OpenDocumentStyleTranslator::translate(const tinyxml2::XMLElement &in, TranslationContext &context) const {
    impl->translate(in, context);
}

}
