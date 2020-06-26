#ifndef ODR_OOXML_PRESENTATION_TRANSLATOR_H
#define ODR_OOXML_PRESENTATION_TRANSLATOR_H

#include <memory>

namespace pugi {
class xml_node;
}

namespace odr {
namespace ooxml {

struct Context;

namespace PresentationTranslator {
void css(const pugi::xml_node &in, Context &context);
void html(const pugi::xml_node &in, Context &context);
} // namespace PresentationTranslator

} // namespace ooxml
} // namespace odr

#endif // ODR_OOXML_PRESENTATION_TRANSLATOR_H
