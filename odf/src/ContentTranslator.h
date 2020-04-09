#ifndef ODR_ODF_CONTENT_TRANSLATOR_H
#define ODR_ODF_CONTENT_TRANSLATOR_H

#include <memory>

namespace tinyxml2 {
class XMLElement;
}

namespace odr {
namespace odf {

struct Context;

namespace ContentTranslator {
void html(const tinyxml2::XMLElement &in, Context &context);
} // namespace ContentTranslator

} // namespace odf
} // namespace odr

#endif // ODR_ODF_CONTENT_TRANSLATOR_H
