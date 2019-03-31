#ifndef ODR_DOCUMENTTRANSLATOR_H
#define ODR_DOCUMENTTRANSLATOR_H

#include <string>
#include <memory>

namespace odr {

class Context;
class OpenDocumentFile;

class DocumentTranslator {
public:
    static std::unique_ptr<DocumentTranslator> create();

    virtual ~DocumentTranslator() = default;
    virtual bool translate(OpenDocumentFile &in, const std::string &out, Context &context) const = 0;
};

}

#endif //ODR_DOCUMENTTRANSLATOR_H
