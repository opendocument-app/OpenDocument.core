#include "Xml2Html.h"
#include "tinyxml2.h"
#include "../TranslationContext.h"

namespace odr {

DefaultElementTranslator::DefaultElementTranslator(const std::string &name)
        : name(name) {
}

void DefaultElementTranslator::addAttributeTranslator(const std::string &from, const std::string &to) {
    simpleAttributeTranslation[from] = to;
}

void DefaultElementTranslator::addAttributes(const std::string &name, const std::string &value) {
    newAttributes.emplace(name, value);
}

void DefaultElementTranslator::translate(const tinyxml2::XMLAttribute &in, odr::TranslationContext &context) const {
    const auto it = simpleAttributeTranslation.find(in.Name());
    if (it != simpleAttributeTranslation.end()) {
        *context.output << " " << it->second << "=\"" << in.Value() << "\"";
        return;
    }
    DefaultXmlTranslator::translate(in, context);
}

void DefaultElementTranslator::translateElementStart(const tinyxml2::XMLElement &in, TranslationContext &context) const {
    *context.output << "<" << name;
}

void DefaultElementTranslator::translateElementAttributes(const tinyxml2::XMLElement &in, TranslationContext &context) const {
    DefaultXmlTranslator::translateElementAttributes(in, context);

    for (auto &&a : newAttributes) {
        *context.output << " " << a.first << "=\"" << a.second << "\"";
    }

    *context.output << ">";
}

void DefaultElementTranslator::translateElementEnd(const tinyxml2::XMLElement &, TranslationContext &context) const {
    *context.output << "</" << name << ">";
}

}
