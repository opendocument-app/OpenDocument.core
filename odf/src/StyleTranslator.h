#ifndef ODR_ODF_STYLE_TRANSLATOR_H
#define ODR_ODF_STYLE_TRANSLATOR_H

#include <Context.h>
#include <memory>

namespace pugi {
class xml_node;
}

namespace odr::odf {

struct Context;

namespace StyleTranslator {
std::string escapeStyleName(const std::string &name);
std::string escapeMasterStyleName(const std::string &name);
void css(const pugi::xml_node &in, Context &context);
} // namespace StyleTranslator

} // namespace odr::odf

#endif // ODR_ODF_STYLE_TRANSLATOR_H
