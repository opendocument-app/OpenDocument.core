#ifndef ODR_OPENDOCUMENTTRANSLATOR_H
#define ODR_OPENDOCUMENTTRANSLATOR_H

#include <string>
#include <memory>

namespace odr {

class OpenDocumentFile;
class TranslationContext;

class OpenDocumentTranslator final {
public:
    OpenDocumentTranslator();
    ~OpenDocumentTranslator();

    bool translate(OpenDocumentFile &in, const std::string &out, TranslationContext &context) const;
    bool backTranslate(OpenDocumentFile &in, const std::string &inHtml, const std::string &out, TranslationContext &context) const;

private:
    class Impl;
    const std::unique_ptr<Impl> impl;
};

}

#endif //ODR_OPENDOCUMENTTRANSLATOR_H
