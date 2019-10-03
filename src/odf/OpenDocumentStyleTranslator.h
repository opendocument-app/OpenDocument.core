#ifndef ODR_OPENDOCUMENTSTYLETRANSLATOR_H
#define ODR_OPENDOCUMENTSTYLETRANSLATOR_H

#include <memory>

namespace tinyxml2 {
class XMLElement;
}

namespace odr {

struct TranslationContext;

class OpenDocumentStyleTranslator final {
public:
    static std::string escapeStyleName(const std::string &);

    OpenDocumentStyleTranslator();
    ~OpenDocumentStyleTranslator();

    void translate(const tinyxml2::XMLElement &in, TranslationContext &context) const;

private:
    class Impl;
    const std::unique_ptr<Impl> impl;
};

}

#endif //ODR_OPENDOCUMENTSTYLETRANSLATOR_H
