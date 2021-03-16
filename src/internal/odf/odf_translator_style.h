#ifndef ODR_INTERNAL_ODF_STYLE_TRANSLATOR_H
#define ODR_INTERNAL_ODF_STYLE_TRANSLATOR_H

#include <memory>

namespace pugi {
class xml_node;
}

namespace odr::internal::odf {
struct Context;

namespace style_translator {
std::string escape_style_name(const std::string &name);
std::string escape_master_style_name(const std::string &name);
void css(const pugi::xml_node &in, Context &context);
} // namespace style_translator

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_STYLE_TRANSLATOR_H
