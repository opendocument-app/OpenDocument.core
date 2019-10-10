#ifndef ODR_XML2HTML_H
#define ODR_XML2HTML_H

#include <string>
#include <unordered_map>
#include "XmlTranslator.h"

namespace odr {

struct TranslationContext;

class DefaultElementTranslator : public DefaultXmlTranslator {
public:
    explicit DefaultElementTranslator(const std::string &name);

    void addAttributeTranslator(const std::string &from, const std::string &to);
    void addAttributes(const std::string &name, const std::string &value);

    using DefaultXmlTranslator::translate;
    void translate(const tinyxml2::XMLAttribute &, TranslationContext &) const override;

protected:
    void translateElementStart(const tinyxml2::XMLElement &, TranslationContext &) const override;
    void translateElementAttributes(const tinyxml2::XMLElement &, TranslationContext &) const override;
    void translateElementEnd(const tinyxml2::XMLElement &, TranslationContext &) const override;

private:
    const std::string name;
    std::unordered_map<std::string, std::string> newAttributes;
    std::unordered_map<std::string, std::string> simpleAttributeTranslation;
};

}

#endif //ODR_XML2HTML_H
