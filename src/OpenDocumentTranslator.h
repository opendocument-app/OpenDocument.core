#ifndef ODR_OPENDOCUMENTTRANSLATOR_H
#define ODR_OPENDOCUMENTTRANSLATOR_H

#include <string>
#include <memory>

namespace odr {

class OpenDocumentFile;
class TranslationContext;

class OpenDocumentTranslator {
public:
    static std::unique_ptr<OpenDocumentTranslator> create();

    virtual ~OpenDocumentTranslator() = default;
    virtual bool translate(OpenDocumentFile &in, const std::string &out, TranslationContext &context) const = 0;
};

}

#endif //ODR_OPENDOCUMENTTRANSLATOR_H
