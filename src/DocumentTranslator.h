#ifndef OPENDOCUMENT_DOCUMENTTRANSLATOR_H
#define OPENDOCUMENT_DOCUMENTTRANSLATOR_H

#include <string>
#include <memory>
#include "StyleTranslator.h"
#include "ContentTranslator.h"

namespace opendocument {

class OpenDocumentFile;

class DocumentTranslator {
public:
    static std::unique_ptr<DocumentTranslator> createDefaultDocumentTranslator();

    virtual ~DocumentTranslator() = default;
    virtual bool translate(OpenDocumentFile &in, const std::string &out) const = 0;
};

}

#endif //OPENDOCUMENT_DOCUMENTTRANSLATOR_H
