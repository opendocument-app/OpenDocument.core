#ifndef ODR_OPENDOCUMENTSTYLETRANSLATOR_H
#define ODR_OPENDOCUMENTSTYLETRANSLATOR_H

#include "OpenDocumentContext.h"
#include "tinyxml2.h"
#include <memory>

namespace odr {

struct TranslationContext;

class OpenDocumentStyleTranslator final {
public:
  static std::string escapeStyleName(const std::string &name);
  static void translate(const tinyxml2::XMLElement &in,
                        TranslationContext &context);
};

} // namespace odr

#endif // ODR_OPENDOCUMENTSTYLETRANSLATOR_H
