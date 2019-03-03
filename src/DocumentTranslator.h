#ifndef OPENDOCUMENT_DOCUMENTTRANSLATOR_H
#define OPENDOCUMENT_DOCUMENTTRANSLATOR_H

#include <string>

namespace opendocument {

class OpenDocumentFile;
class StyleTranslator;
class ContentTranslator;

class DocumentTranslator {
public:
    virtual bool translate(OpenDocumentFile &in, const std::string &out) const;

protected:
    virtual const StyleTranslator &getStyleTranslator() const = 0;
    virtual const ContentTranslator &getContentTranslator() const = 0;
};

}

#endif //OPENDOCUMENT_DOCUMENTTRANSLATOR_H
