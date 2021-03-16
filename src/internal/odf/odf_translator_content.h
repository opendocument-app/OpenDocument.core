#ifndef ODR_INTERNAL_ODF_CONTENT_TRANSLATOR_H
#define ODR_INTERNAL_ODF_CONTENT_TRANSLATOR_H

#include <memory>

namespace pugi {
class xml_node;
}

namespace odr::internal::odf {
struct Context;

namespace content_translator {
void html(const pugi::xml_node &in, Context &context);
} // namespace content_translator

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_CONTENT_TRANSLATOR_H
