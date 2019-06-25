#ifndef ODR_OPENDOCUMENTSTYLETRANSLATOR_H
#define ODR_OPENDOCUMENTSTYLETRANSLATOR_H

#include <memory>

namespace tinyxml2 {
class XMLElement;
}

namespace odr {

struct OpenDocumentContext;

class OpenDocumentStyleTranslator {
public:
    static std::unique_ptr<OpenDocumentStyleTranslator> create();

    virtual ~OpenDocumentStyleTranslator() = default;
    virtual void translate(const tinyxml2::XMLElement &in, OpenDocumentContext &context) const = 0;
};

}

#endif //ODR_OPENDOCUMENTSTYLETRANSLATOR_H
