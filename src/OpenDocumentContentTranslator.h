#ifndef ODR_OPENDOCUMENTCONTENTTRANSLATOR_H
#define ODR_OPENDOCUMENTCONTENTTRANSLATOR_H

#include <memory>

namespace tinyxml2 {
class XMLElement;
}

namespace odr {

struct TranslationContext;

class OpenDocumentContentTranslator {
public:
    static std::unique_ptr<OpenDocumentContentTranslator> create();

    virtual ~OpenDocumentContentTranslator() = default;
    virtual void translate(const tinyxml2::XMLElement &in, TranslationContext &context) const = 0;
};

}

#endif //ODR_OPENDOCUMENTCONTENTTRANSLATOR_H
