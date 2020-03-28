#ifndef ODR_OPENDOCUMENTCONTENTTRANSLATOR_H
#define ODR_OPENDOCUMENTCONTENTTRANSLATOR_H

#include <memory>

namespace tinyxml2 {
class XMLElement;
}

namespace odr {
namespace common {
struct TranslationContext;
}
} // namespace odr

namespace odr {

class OpenDocumentContentTranslator final {
public:
  static void translate(const tinyxml2::XMLElement &in,
                        common::TranslationContext &context);
};

} // namespace odr

#endif // ODR_OPENDOCUMENTCONTENTTRANSLATOR_H
