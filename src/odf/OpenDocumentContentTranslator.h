#ifndef ODR_OPENDOCUMENTCONTENTTRANSLATOR_H
#define ODR_OPENDOCUMENTCONTENTTRANSLATOR_H

#include <memory>

namespace tinyxml2 {
class XMLElement;
}

namespace odr {

struct TranslationContext;

class OpenDocumentContentTranslator final {
public:
    OpenDocumentContentTranslator();
    ~OpenDocumentContentTranslator();

    void translate(const tinyxml2::XMLElement &in, TranslationContext &context) const;

private:
    class Impl;
    const std::unique_ptr<Impl> impl;
};

}

#endif //ODR_OPENDOCUMENTCONTENTTRANSLATOR_H
