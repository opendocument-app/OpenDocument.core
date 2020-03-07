#ifndef ODR_OPENDOCUMENTSTYLETRANSLATOR_H
#define ODR_OPENDOCUMENTSTYLETRANSLATOR_H

#include <memory>
#include "tinyxml2.h"
#include "OpenDocumentContext.h"

namespace odr {

struct TranslationContext;

class OpenDocumentStyleTranslator final {
public:
    static std::string escapeStyleName(const std::string &name);
    static void translate(const tinyxml2::XMLElement &in, TranslationContext &context);
};

}

#endif //ODR_OPENDOCUMENTSTYLETRANSLATOR_H
