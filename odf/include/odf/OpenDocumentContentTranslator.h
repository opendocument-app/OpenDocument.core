#ifndef ODR_OPENDOCUMENTCONTENTTRANSLATOR_H
#define ODR_OPENDOCUMENTCONTENTTRANSLATOR_H

#include <memory>

namespace tinyxml2 {
class XMLElement;
}

namespace odr {

struct TranslationContext;

class OpenDocumentContentTranslator final {
public:
  static void translate(const tinyxml2::XMLElement &in,
                        TranslationContext &context);
};

} // namespace odr

#endif // ODR_OPENDOCUMENTCONTENTTRANSLATOR_H
