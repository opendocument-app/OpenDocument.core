#ifndef ODR_ODF_CONTENT_TRANSLATOR_H
#define ODR_ODF_CONTENT_TRANSLATOR_H

#include <memory>

namespace pugi {
class xml_node;
}

namespace odr::odf {

struct Context;

namespace ContentTranslator {
void html(const pugi::xml_node &in, Context &context);
} // namespace ContentTranslator

} // namespace odr::odf

#endif // ODR_ODF_CONTENT_TRANSLATOR_H
