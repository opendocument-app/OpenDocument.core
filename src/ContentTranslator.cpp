#include "ContentTranslator.h"
#include <string>
#include <unordered_map>
#include "tinyxml2.h"

namespace opendocument {

class DefaultContentTranslatorImpl : public ContentTranslator {
public:
    std::unordered_map<std::string, std::string> elementTranslation;

    DefaultContentTranslatorImpl() {
        elementTranslation["text:p"] = "p"; // https://github.com/andiwand/OpenDocument.java/blob/master/src/at/stefl/opendocument/java/translator/content/ParagraphTranslator.java
        elementTranslation["text:h"] = "h";
        elementTranslation["text:span"] = "span"; // https://github.com/andiwand/OpenDocument.java/blob/master/src/at/stefl/opendocument/java/translator/content/SpanTranslator.java
        elementTranslation["text:s"] = ""; // https://github.com/andiwand/OpenDocument.java/blob/master/src/at/stefl/opendocument/java/translator/content/SpaceTranslator.java
        elementTranslation["text:tab"] = ""; // https://github.com/andiwand/OpenDocument.java/blob/master/src/at/stefl/opendocument/java/translator/content/TabTranslator.java
        elementTranslation["text:line-break"] = "br";
        elementTranslation["text:list"] = "ul";
        elementTranslation["text:list-item"] = "li";
    }
    ~DefaultContentTranslatorImpl() override = default;

    bool translate(tinyxml2::XMLElement &in, std::ostream &out) const override {
        auto elementTranslationIt = elementTranslation.find(in.Name());
        if (elementTranslationIt != elementTranslation.end()) {
            out << "<" << elementTranslationIt->second << ">";
        }

        for (auto child = in.FirstChild(); child != nullptr; child = child->NextSibling()) {
            if (child->ToText() != nullptr) {
                out << child->ToText()->Value();
            } else if (child->ToElement() != nullptr) {
                translate(*child->ToElement(), out);
            }
        }

        if (elementTranslationIt != elementTranslation.end()) {
            out << "</" << elementTranslationIt->second << ">";
        }

        return true;
    }
};

std::unique_ptr<ContentTranslator> ContentTranslator::createDefaultContentTranslator() {
    return std::unique_ptr<DefaultContentTranslatorImpl>();
}

}
