#ifndef ODR_ODF_CONTENT_TRANSLATOR_H
#define ODR_ODF_CONTENT_TRANSLATOR_H

#include <memory>

namespace pugi {
class xml_node;
}

namespace odr {
namespace odf {

struct Context;

namespace ContentTranslator {
void html(const pugi::xml_node &in, Context &context);
} // namespace ContentTranslator

} // namespace odf
} // namespace odr

#endif // ODR_ODF_CONTENT_TRANSLATOR_H
