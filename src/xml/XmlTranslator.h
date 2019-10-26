#ifndef ODR_XMLTRANSLATOR_H
#define ODR_XMLTRANSLATOR_H

#include <string>
#include <unordered_map>

namespace tinyxml2 {
class XMLNode;
class XMLElement;
class XMLAttribute;
class XMLText;
}

namespace odr {

struct TranslationContext;

class XmlElementTranslator {
public:
    virtual ~XmlElementTranslator() = default;
    virtual void translate(const tinyxml2::XMLElement &, TranslationContext &) const = 0;
};

class XmlAttributeTranslator {
public:
    virtual ~XmlAttributeTranslator() = default;
    virtual void translate(const tinyxml2::XMLAttribute &, TranslationContext &) const = 0;
};

class XmlTextTranslator {
public:
    virtual ~XmlTextTranslator() = default;
    virtual void translate(const tinyxml2::XMLText &, TranslationContext &) const = 0;
};

class XmlTranslator : public XmlElementTranslator, public XmlAttributeTranslator, public XmlTextTranslator {
public:
    ~XmlTranslator() override = default;

    virtual void translate(const tinyxml2::XMLNode &, TranslationContext &) const;
    void translate(const tinyxml2::XMLElement &, TranslationContext &) const override = 0;
    void translate(const tinyxml2::XMLAttribute &, TranslationContext &) const override = 0;
    void translate(const tinyxml2::XMLText &, TranslationContext &) const override = 0;
};

class DefaultXmlTranslator : public XmlTranslator {
public:
    DefaultXmlTranslator();

    DefaultXmlTranslator &setDefaultDelegation(XmlTranslator *);
    DefaultXmlTranslator &addAttributeDelegation(const std::string &name, XmlTranslator &);
    DefaultXmlTranslator &addElementDelegation(const std::string &name, XmlTranslator &);

    void translate(const tinyxml2::XMLNode &, TranslationContext &) const override;
    void translate(const tinyxml2::XMLElement &, TranslationContext &) const override;
    void translate(const tinyxml2::XMLAttribute &, TranslationContext &) const override;
    void translate(const tinyxml2::XMLText &, TranslationContext &) const override;

protected:
    virtual void translateElementStart(const tinyxml2::XMLElement &, TranslationContext &) const {}
    virtual void translateElementAttributes(const tinyxml2::XMLElement &, TranslationContext &) const;
    virtual void translateElementChildren(const tinyxml2::XMLElement &, TranslationContext &) const;
    virtual void translateElementChild(const tinyxml2::XMLElement &, TranslationContext &) const;
    virtual void translateElementEnd(const tinyxml2::XMLElement &, TranslationContext &) const {}

private:
    XmlTranslator *defaultDelegation;
    std::unordered_map<std::string, const XmlAttributeTranslator &> attributeDelegation;
    std::unordered_map<std::string, const XmlElementTranslator &> elementDelegation;
};

}

#endif //ODR_XMLTRANSLATOR_H
