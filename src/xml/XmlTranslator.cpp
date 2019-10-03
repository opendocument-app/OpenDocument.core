#include "XmlTranslator.h"
#include "tinyxml2.h"

namespace odr {

void XmlTranslator::translate(const tinyxml2::XMLNode &in, TranslationContext &context) const {
    if (in.ToElement() != nullptr) {
        translate((tinyxml2::XMLElement &) in, context);
    } else if (in.ToText() != nullptr) {
        translate((tinyxml2::XMLText &) in, context);
    }
}

DefaultXmlTranslator::DefaultXmlTranslator() :
        defaultDelegation(nullptr) {
}

DefaultXmlTranslator &DefaultXmlTranslator::setDefaultDelegation(XmlTranslator *translator) {
    defaultDelegation = translator;
    return *this;
}

DefaultXmlTranslator &DefaultXmlTranslator::addAttributeDelegation(const std::string &name, XmlTranslator &translator) {
    attributeDelegation.emplace(name, translator);
    return *this;
}

DefaultXmlTranslator &DefaultXmlTranslator::addElementDelegation(const std::string &name, XmlTranslator &translator) {
    elementDelegation.emplace(name, translator);
    return *this;
}

void DefaultXmlTranslator::translate(const tinyxml2::XMLNode &in, TranslationContext &context) const {
    if (in.ToElement() != nullptr) {
        translate((tinyxml2::XMLElement &) in, context);
    } else if (in.ToText() != nullptr) {
        translate((tinyxml2::XMLText &) in, context);
    }
}

void DefaultXmlTranslator::translate(const tinyxml2::XMLAttribute &in, TranslationContext &context) const {
    const std::string name(in.Name());

    auto it = attributeDelegation.find(name);
    if (it != attributeDelegation.end()) {
        it->second.translate(in, context);
        return;
    }

    if (defaultDelegation != nullptr) {
        defaultDelegation->translate(in, context);
        return;
    }
}

void DefaultXmlTranslator::translate(const tinyxml2::XMLText &in, TranslationContext &context) const {
    if (defaultDelegation != nullptr) {
        defaultDelegation->translate(in, context);
    }
}

void DefaultXmlTranslator::translate(const tinyxml2::XMLElement &in, TranslationContext &context) const {
    translateElementStart(in, context);
    translateElementAttributes(in, context);
    translateElementChildren(in, context);
    translateElementEnd(in, context);
}

void DefaultXmlTranslator::translateElementAttributes(const tinyxml2::XMLElement &in, TranslationContext &context) const {
    for (auto attr = in.FirstAttribute(); attr != nullptr; attr = attr->Next()) {
        translate(*attr, context);
    }
}

void DefaultXmlTranslator::translateElementChildren(const tinyxml2::XMLElement &in, TranslationContext &context) const {
    for (auto node = in.FirstChild(); node != nullptr; node = node->NextSibling()) {
        if (node->ToElement() != nullptr) {
            translateElementChild(*node->ToElement(), context);
        } else if (node->ToText() != nullptr) {
            translate(*node->ToText(), context);
        }
    }
}

void DefaultXmlTranslator::translateElementChild(const tinyxml2::XMLElement &in, TranslationContext &context) const {
    const std::string name(in.Name());

    auto it = elementDelegation.find(name);
    if (it != elementDelegation.end()) {
        it->second.translate(in, context);
        return;
    }

    if (defaultDelegation != nullptr) {
        defaultDelegation->translate(in, context);
        return;
    }
}

}
