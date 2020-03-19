#ifndef ODR_OFFICEOPENXMLDOCUMENTTRANSLATOR_H
#define ODR_OFFICEOPENXMLDOCUMENTTRANSLATOR_H

#include <memory>

namespace tinyxml2 {
class XMLElement;
}

namespace odr {

struct TranslationContext;

class OfficeOpenXmlDocumentTranslator {
public:
  static void translateStyle(const tinyxml2::XMLElement &in,
                             TranslationContext &context);
  static void translateContent(const tinyxml2::XMLElement &in,
                               TranslationContext &context);
};

} // namespace odr

#endif // ODR_OFFICEOPENXMLDOCUMENTTRANSLATOR_H
