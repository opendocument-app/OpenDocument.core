#ifndef OPENDOCUMENT_DOCUMENTTRANSLATOR_H
#define OPENDOCUMENT_DOCUMENTTRANSLATOR_H

#include <string>
#include <memory>
#include "ContentTranslator.h"
#include "StyleTranslator.h"

namespace opendocument {

class OpenDocumentFile;

class DocumentTranslator {
public:
    virtual ~DocumentTranslator() = default;
    virtual bool translate(OpenDocumentFile &in, const std::string &out) const;

protected:
    virtual const StyleTranslator &getStyleTranslator() const = 0;
    virtual const ContentTranslator &getContentTranslator() const = 0;
};

class TextDocumentTranslator : public DocumentTranslator {
public:
    ~TextDocumentTranslator() override = default;

protected:
    const StyleTranslator &getStyleTranslator() const override;
    const ContentTranslator &getContentTranslator() const override;

private:
    StyleTranslator styleTranslator_;
    TextContentTranslator contentTranslator_;
};

}

#endif //OPENDOCUMENT_DOCUMENTTRANSLATOR_H
