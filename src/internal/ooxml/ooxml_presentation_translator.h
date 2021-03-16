#ifndef ODR_INTERNAL_OOXML_PRESENTATION_TRANSLATOR_H
#define ODR_INTERNAL_OOXML_PRESENTATION_TRANSLATOR_H

namespace pugi {
class xml_node;
}

namespace odr::internal::ooxml {
struct Context;

namespace presentation_translator {
void css(const pugi::xml_node &in, Context &context);
void html(const pugi::xml_node &in, Context &context);
} // namespace presentation_translator

} // namespace odr::internal::ooxml

#endif // ODR_INTERNAL_OOXML_PRESENTATION_TRANSLATOR_H
