#ifndef ODR_OPENDOCUMENTSTYLETRANSLATOR_H
#define ODR_OPENDOCUMENTSTYLETRANSLATOR_H

#include <memory>

namespace tinyxml2 {
class XMLElement;
}

namespace odr {

struct TranslationContext;

class OpenDocumentStyleTranslator {
public:
    static std::unique_ptr<OpenDocumentStyleTranslator> create();
    static std::string escapeStyleName(const std::string &);

    virtual ~OpenDocumentStyleTranslator() = default;
    virtual void translate(const tinyxml2::XMLElement &in, TranslationContext &context) const = 0;
};

}

#endif //ODR_OPENDOCUMENTSTYLETRANSLATOR_H
