#ifndef ODR_OFFICEOPENXMLPRESENTATIONTRANSLATOR_H
#define ODR_OFFICEOPENXMLPRESENTATIONTRANSLATOR_H

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
namespace ooxml {

class OfficeOpenXmlPresentationTranslator {
public:
  static void translateStyle(const tinyxml2::XMLElement &in,
                             common::TranslationContext &context);
  static void translateContent(const tinyxml2::XMLElement &in,
                               common::TranslationContext &context);
};

} // namespace ooxml
} // namespace odr

#endif // ODR_OFFICEOPENXMLPRESENTATIONTRANSLATOR_H
