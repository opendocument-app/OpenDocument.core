#include "OpenDocumentStyleTranslator.h"
#include <string>
#include <unordered_set>
#include <unordered_map>
#include "tinyxml2.h"
#include "glog/logging.h"
#include "odr/TranslationConfig.h"
#include "OpenDocumentContext.h"

namespace odr {

namespace {

class StyleElementTranslator {
public:
    virtual ~StyleElementTranslator() = default;
    virtual void translate(const tinyxml2::XMLElement &in, std::ostream &out, OpenDocumentContext &context) const = 0;
};

class StyleClassTranslator : public StyleElementTranslator {
public:
    std::string nameAttribute;
    std::unordered_set<std::string> properties;
    std::unordered_map<std::string, const char *> attributeTranslator;

    explicit StyleClassTranslator(const std::string &nameAttribute)
            : nameAttribute(nameAttribute) {
        // TODO: there should be a config file for this

        properties.insert("style:text-properties");
        properties.insert("style:paragraph-properties");
        properties.insert("style:graphic-properties");
        properties.insert("style:table-properties");
        properties.insert("style:table-column-properties");
        properties.insert("style:table-row-properties");
        properties.insert("style:table-cell-properties");
        properties.insert("style:page-layout-properties");
        properties.insert("style:section-properties");
        properties.insert("style:drawing-page-properties");
        properties.insert("loext:graphic-properties");

        attributeTranslator["fo:font-family"] = nullptr;
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
        attributeTranslator["fo:margin"] = nullptr;
        attributeTranslator["fo:margin-top"] = "margin-top";
        attributeTranslator["fo:margin-right"] = "margin-right";
        attributeTranslator["fo:margin-bottom"] = "margin-bottom";
        attributeTranslator["fo:margin-left"] = "margin-left";
        attributeTranslator["fo:padding"] = "padding";
        attributeTranslator["fo:padding-top"] = "padding-top";
        attributeTranslator["fo:padding-right"] = "padding-right";
        attributeTranslator["fo:padding-bottom"] = "padding-bottom";
        attributeTranslator["fo:padding-left"] = "padding-left";
        attributeTranslator["fo:font-variant"] = nullptr;
        attributeTranslator["fo:text-transform"] = nullptr;
        attributeTranslator["fo:line-height"] = nullptr;
        attributeTranslator["fo:keep-together"] = nullptr;
        attributeTranslator["fo:orphans"] = nullptr;
        attributeTranslator["fo:widows"] = nullptr;
        attributeTranslator["fo:text-indent"] = nullptr;
        attributeTranslator["fo:keep-with-next"] = nullptr;
        attributeTranslator["fo:break-before"] = nullptr;
        attributeTranslator["fo:wrap-option"] = nullptr;
        attributeTranslator["fo:language"] = nullptr;
        attributeTranslator["fo:country"] = nullptr;
        attributeTranslator["fo:hyphenation-ladder-count"] = nullptr;
        attributeTranslator["fo:hyphenation-remain-char-count"] = nullptr;
        attributeTranslator["fo:hyphenation-push-char-count"] = nullptr;
        attributeTranslator["fo:hyphenate"] = nullptr;
        attributeTranslator["fo:clip"] = nullptr;
        attributeTranslator["fo:border"] = "border";
        attributeTranslator["fo:border-top"] = "border-top";
        attributeTranslator["fo:border-right"] = "border-right";
        attributeTranslator["fo:border-bottom"] = "border-bottom";
        attributeTranslator["fo:border-left"] = "border-left";
        attributeTranslator["fo:min-width"] = nullptr;
        attributeTranslator["fo:max-width"] = nullptr;
        attributeTranslator["fo:min-height"] = nullptr;
        attributeTranslator["fo:max-height"] = nullptr;

        attributeTranslator["style:font-name"] = "font-family";
        attributeTranslator["style:width"] = "width";
        attributeTranslator["style:height"] = "height";
        attributeTranslator["style:vertical-align"] = "vertical-align";
        attributeTranslator["style:column-width"] = "width";
        attributeTranslator["style:row-height"] = "height";
        attributeTranslator["style:shadow"] = nullptr;
        attributeTranslator["style:text-position"] = nullptr;
        attributeTranslator["style:text-underline-style"] = nullptr;
        attributeTranslator["style:text-line-through-style"] = nullptr;
        attributeTranslator["style:text-line-through-type"] = nullptr;
        attributeTranslator["style:font-name-asian"] = nullptr;
        attributeTranslator["style:font-family-asian"] = nullptr;
        attributeTranslator["style:font-family-generic-asian"] = nullptr;
        attributeTranslator["style:font-pitch-asian"] = nullptr;
        attributeTranslator["style:font-size-asian"] = nullptr;
        attributeTranslator["style:font-style-name"] = nullptr;
        attributeTranslator["style:font-style-asian"] = nullptr;
        attributeTranslator["style:font-style-complex"] = nullptr;
        attributeTranslator["style:font-weight-asian"] = nullptr;
        attributeTranslator["style:font-name-complex"] = nullptr;
        attributeTranslator["style:font-family-generic"] = nullptr;
        attributeTranslator["style:font-family-complex"] = nullptr;
        attributeTranslator["style:font-family-generic-complex"] = nullptr;
        attributeTranslator["style:font-pitch-complex"] = nullptr;
        attributeTranslator["style:font-size-complex"] = nullptr;
        attributeTranslator["style:justify-single-word"] = nullptr;
        attributeTranslator["style:auto-text-indent"] = nullptr;
        attributeTranslator["style:wrap"] = nullptr;
        attributeTranslator["style:number-wrapped-paragraphs"] = nullptr;
        attributeTranslator["style:wrap-contour"] = nullptr;
        attributeTranslator["style:vertical-pos"] = nullptr;
        attributeTranslator["style:vertical-rel"] = nullptr;
        attributeTranslator["style:horizontal-pos"] = nullptr;
        attributeTranslator["style:horizontal-rel"] = nullptr;
        attributeTranslator["style:font-pitch"] = nullptr;
        attributeTranslator["style:flow-with-text"] = nullptr;
        attributeTranslator["style:text-autospace"] = nullptr;
        attributeTranslator["style:line-break"] = nullptr;
        attributeTranslator["style:font-independent-line-spacing"] = nullptr;
        attributeTranslator["style:use-window-font-color"] = nullptr;
        attributeTranslator["style:letter-kerning"] = nullptr;
        attributeTranslator["style:language-asian"] = nullptr;
        attributeTranslator["style:language-complex"] = nullptr;
        attributeTranslator["style:country-asian"] = nullptr;
        attributeTranslator["style:country-complex"] = nullptr;
        attributeTranslator["style:punctuation-wrap"] = nullptr;
        attributeTranslator["style:tab-stop-distance"] = nullptr;
        attributeTranslator["style:writing-mode"] = nullptr;
        attributeTranslator["style:page-number"] = nullptr;
        attributeTranslator["style:run-through"] = nullptr;
        attributeTranslator["style:mirror"] = nullptr;
        attributeTranslator["style:decimal-places"] = nullptr;
        attributeTranslator["style:rotation-align"] = nullptr;
        attributeTranslator["style:text-underline-width"] = nullptr;
        attributeTranslator["style:text-underline-color"] = nullptr;
        attributeTranslator["style:text-align-source"] = nullptr;
        attributeTranslator["style:repeat-content"] = nullptr;
        attributeTranslator["style:rotation-angle"] = nullptr;
        attributeTranslator["style:cell-protect"] = nullptr;
        attributeTranslator["style:print-content"] = nullptr;
        attributeTranslator["style:diagonal-bl-tr"] = nullptr;
        attributeTranslator["style:diagonal-tl-br"] = nullptr;
        attributeTranslator["style:direction"] = nullptr;
        attributeTranslator["style:shrink-to-fit"] = nullptr;
        attributeTranslator["style:text-outline"] = nullptr;
        attributeTranslator["style:font-weight-complex"] = nullptr;
        attributeTranslator["style:use-optimal-row-height"] = nullptr;
        attributeTranslator["style:text-underline-mode"] = nullptr;
        attributeTranslator["style:text-overline-mode"] = nullptr;
        attributeTranslator["style:text-line-through-mode"] = nullptr;
        attributeTranslator["style:text-emphasize"] = nullptr;
        attributeTranslator["style:font-relief"] = nullptr;
        attributeTranslator["style:text-overline-style"] = nullptr;
        attributeTranslator["style:text-overline-color"] = nullptr;
        attributeTranslator["style:glyph-orientation-vertical"] = nullptr;
        attributeTranslator["style:protect"] = nullptr;
        attributeTranslator["style:rel-column-width"] = nullptr;

        attributeTranslator["text:anchor-type"] = nullptr;
        attributeTranslator["text:number-lines"] = nullptr;
        attributeTranslator["text:line-lines"] = nullptr;
        attributeTranslator["text:line-number"] = nullptr;

        attributeTranslator["table:align"] = nullptr;
        attributeTranslator["table:border-model"] = nullptr;
        attributeTranslator["table:display"] = nullptr;

        attributeTranslator["draw:stroke"] = nullptr;
        attributeTranslator["draw:fill"] = nullptr;
        attributeTranslator["draw:fill-color"] = nullptr;
        attributeTranslator["draw:luminance"] = nullptr;
        attributeTranslator["draw:contrast"] = nullptr;
        attributeTranslator["draw:red"] = nullptr;
        attributeTranslator["draw:green"] = nullptr;
        attributeTranslator["draw:blue"] = nullptr;
        attributeTranslator["draw:gamma"] = nullptr;
        attributeTranslator["draw:shadow"] = nullptr;
        attributeTranslator["draw:color-inversion"] = nullptr;
        attributeTranslator["draw:color-mode"] = nullptr;
        attributeTranslator["draw:image-opacity"] = nullptr;
        attributeTranslator["draw:shadow-offset-x"] = nullptr;
        attributeTranslator["draw:shadow-offset-y"] = nullptr;
        attributeTranslator["draw:start-line-spacing-horizontal"] = nullptr;
        attributeTranslator["draw:start-line-spacing-vertical"] = nullptr;
        attributeTranslator["draw:end-line-spacing-horizontal"] = nullptr;
        attributeTranslator["draw:end-line-spacing-vertical"] = nullptr;
        attributeTranslator["draw:marker-start"] = nullptr;
        attributeTranslator["draw:marker-start-width"] = nullptr;
        attributeTranslator["draw:marker-start-center"] = nullptr;
        attributeTranslator["draw:marker-end"] = nullptr;
        attributeTranslator["draw:marker-end-width"] = nullptr;
        attributeTranslator["draw:auto-grow-width"] = nullptr;
        attributeTranslator["draw:auto-grow-height"] = nullptr;
        attributeTranslator["draw:caption-escape-direction"] = nullptr;
        attributeTranslator["draw:textarea-horizontal-align"] = nullptr;
        attributeTranslator["draw:textarea-vertical-align"] = nullptr;
        attributeTranslator["draw:ole-draw-aspect"] = nullptr;
        attributeTranslator["draw:shadow-color"] = nullptr;
        attributeTranslator["draw:fit-to-size"] = nullptr;
        attributeTranslator["draw:show-unit"] = nullptr;
        attributeTranslator["draw:marker-end-center"] = nullptr;

        attributeTranslator["presentation:background-visible"] = nullptr;
        attributeTranslator["presentation:background-objects-visible"] = nullptr;
        attributeTranslator["presentation:display-footer"] = nullptr;
        attributeTranslator["presentation:display-page-number"] = nullptr;
        attributeTranslator["presentation:display-date-time"] = nullptr;
        attributeTranslator["presentation:display-header"] = nullptr;
        attributeTranslator["presentation:display-footer"] = nullptr;

        attributeTranslator["svg:x"] = nullptr;
        attributeTranslator["svg:y"] = nullptr;
        attributeTranslator["svg:stroke-color"] = nullptr;
        attributeTranslator["svg:stroke-width"] = nullptr;

        attributeTranslator["tableooo:tab-color"] = nullptr;
        attributeTranslator["officeooo:rsid"] = nullptr;
        attributeTranslator["officeooo:paragraph-rsid"] = nullptr;
        attributeTranslator["loext:contextual-spacing"] = nullptr;
    }

    ~StyleClassTranslator() override = default;

    void translate(const tinyxml2::XMLElement &in, std::ostream &out, OpenDocumentContext &context) const override {
        auto styleNameAttr = in.FindAttribute(nameAttribute.c_str());
        if (styleNameAttr == nullptr) {
            LOG(WARNING) << "skipped style " << in.Name() << ". no name attribute.";
            return;
        }
        const std::string styleName = OpenDocumentStyleTranslator::escapeStyleName(styleNameAttr->Value());

        const char *parentStyleName;
        if (in.QueryStringAttribute("style:parent-style-name", &parentStyleName) == tinyxml2::XML_SUCCESS) {
            context.styleDependencies[styleName].push_back(OpenDocumentStyleTranslator::escapeStyleName(parentStyleName));
        }
        const char *family;
        if (in.QueryStringAttribute("style:family", &family) == tinyxml2::XML_SUCCESS) {
            context.styleDependencies[styleName].push_back(OpenDocumentStyleTranslator::escapeStyleName(family));
        }

        out << "." << styleName << "{";

        for (auto child = in.FirstChild(); child != nullptr; child = child->NextSibling()) {
            if (child->ToElement() == nullptr) {
                LOG(WARNING) << "skipped. no element.";
                continue;
            }

            const std::string propertyName = child->ToElement()->Name();
            if (properties.find(propertyName) == properties.end()) {
                LOG(WARNING) << "unhandled property " << propertyName;
                continue;
            }

            translateAttributes(*child->ToElement(), out, context);

            if (!child->ToElement()->NoChildren()) {
                //tinyxml2::XMLPrinter printer;
                //child->ToElement()->Accept(&printer);
                LOG(WARNING) << "unhandled children in " << child->ToElement()->Name();
                continue;
            }
        }

        out << "}\n";
    }

    void translateAttributes(const tinyxml2::XMLElement &in, std::ostream &out, OpenDocumentContext &context) const {
        for (auto attr = in.FirstAttribute(); attr != nullptr; attr = attr->Next()) {
            translateAttribute(in, *attr, out, context);
        }
    }

    void translateAttribute(const tinyxml2::XMLElement &e, const tinyxml2::XMLAttribute &in, std::ostream &out, OpenDocumentContext &context) const {
        const std::string attributeName = in.Name();
        auto attributeTranslatorIt = attributeTranslator.find(attributeName);
        if (attributeTranslatorIt == attributeTranslator.end()) {
            LOG(WARNING) << "unhandled style attribute: " << attributeName;
            return;
        }
        auto translator = attributeTranslatorIt->second;
        if (translator == nullptr) {
            return;
        }

        out << translator << ":" << in.Value() << ";";
    }
};

class DefaultStyleTranslator : public OpenDocumentStyleTranslator {
public:
    std::unordered_map<std::string, std::unique_ptr<StyleElementTranslator>> elementTranslator;

    DefaultStyleTranslator() {
        elementTranslator["style:font-face"] = nullptr;
        elementTranslator["style:default-style"] = std::make_unique<StyleClassTranslator>("style:family");
        elementTranslator["style:style"] = std::make_unique<StyleClassTranslator>("style:name");
        elementTranslator["style:default-page-layout"] = nullptr;
        elementTranslator["style:page-layout"] = nullptr;
        elementTranslator["style:presentation-page-layout"] = nullptr;

        elementTranslator["text:outline-style"] = nullptr;
        elementTranslator["text:list-style"] = nullptr;
        elementTranslator["text:notes-configuration"] = nullptr;
        elementTranslator["text:linenumbering-configuration"] = nullptr;
        elementTranslator["text:bibliography-configuration"] = nullptr;

        elementTranslator["number:text-style"] = nullptr;
        elementTranslator["number:number-style"] = nullptr;
        elementTranslator["number:time-style"] = nullptr;
        elementTranslator["number:date-style"] = nullptr;
        elementTranslator["number:currency-style"] = nullptr;

        elementTranslator["draw:marker"] = nullptr;
    }

    ~DefaultStyleTranslator() override = default;

    void translate(const tinyxml2::XMLElement &in, OpenDocumentContext &context) const override {
        auto &out = *context.output;
        context.currentElement = &in;

        for (auto child = in.FirstChild(); child != nullptr; child = child->NextSibling()) {
            if (child->ToElement() == nullptr) {
                LOG(WARNING) << "skipped. no element.";
                continue;
            }

            const std::string elementName = child->ToElement()->Name();
            auto elementTranslatorIt = elementTranslator.find(elementName);
            const StyleElementTranslator *translator = nullptr;
            if (elementTranslatorIt != elementTranslator.end()) {
                translator = elementTranslatorIt->second.get();
            } else {
                LOG(WARNING) << "unhandled style element: " << elementName;
            }

            if (translator != nullptr) {
                context.currentElement = child->ToElement();
                translator->translate(*context.currentElement, out, context);
            }
        }
    }
};

}

// TODO: out-source
static void findAndReplaceAll(std::string &data, const std::string &toSearch, const std::string &replaceStr) {
    std::size_t pos = data.find(toSearch);
    while (pos != std::string::npos) {
        data.replace(pos, toSearch.size(), replaceStr);
        pos = data.find(toSearch, pos + replaceStr.size());
    }
}

std::string OpenDocumentStyleTranslator::escapeStyleName(const std::string &name) {
    std::string result = name;
    findAndReplaceAll(result, ".", "_");
    return result;
}

std::unique_ptr<OpenDocumentStyleTranslator> OpenDocumentStyleTranslator::create() {
    return std::make_unique<DefaultStyleTranslator>();
}

}
