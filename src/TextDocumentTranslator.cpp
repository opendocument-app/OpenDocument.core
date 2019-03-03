#include "TextDocumentTranslator.h"

namespace opendocument {

const StyleTranslator& TextDocumentTranslator::getStyleTranslator() const {
    return styleTranslator_;
}

const ContentTranslator& TextDocumentTranslator::getContentTranslator() const {
    return contentTranslator_;
}

}
