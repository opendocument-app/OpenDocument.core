#ifndef ODR_DOCUMENTTRANSLATOR_H
#define ODR_DOCUMENTTRANSLATOR_H

#include <string>
#include <memory>

namespace odr {

class TranslationConfig;
class OpenDocumentFile;

class DocumentTranslator {
public:
    static std::unique_ptr<DocumentTranslator> create(const TranslationConfig &);

    virtual ~DocumentTranslator() = default;
    virtual bool translate(OpenDocumentFile &in, const std::string &out) const = 0;
};

}

#endif //ODR_DOCUMENTTRANSLATOR_H
