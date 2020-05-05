#ifndef ODR_ODF_STYLE_TRANSLATOR_H
#define ODR_ODF_STYLE_TRANSLATOR_H

#include <Context.h>
#include <memory>
#include <tinyxml2.h>

namespace odr {
namespace odf {

struct Context;

namespace StyleTranslator {
std::string escapeStyleName(const std::string &name);
std::string escapeMasterStyleName(const std::string &name);
void css(const tinyxml2::XMLElement &in, Context &context);
} // namespace StyleTranslator

} // namespace odf
} // namespace odr

#endif // ODR_ODF_STYLE_TRANSLATOR_H
