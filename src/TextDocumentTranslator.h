#ifndef OPENDOCUMENT_TEXTDOCUMENTTRANSLATOR_H
#define OPENDOCUMENT_TEXTDOCUMENTTRANSLATOR_H

#include "DocumentTranslator.h"
#include "StyleTranslator.h"
#include "TextContentTranslator.h"

namespace opendocument {

class TextDocumentTranslator : public DocumentTranslator {
public:

protected:
    const StyleTranslator &getStyleTranslator() const override;
    const ContentTranslator &getContentTranslator() const override;

private:
    StyleTranslator styleTranslator_;
    TextContentTranslator contentTranslator_;
};

}

#endif //OPENDOCUMENT_TEXTDOCUMENTTRANSLATOR_H
