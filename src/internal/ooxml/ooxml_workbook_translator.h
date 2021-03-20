#ifndef ODR_INTERNAL_OOXML_WORKBOOK_TRANSLATOR_H
#define ODR_INTERNAL_OOXML_WORKBOOK_TRANSLATOR_H

namespace pugi {
class xml_node;
}

namespace odr::internal::ooxml {
struct Context;

namespace workbook_translator {
void css(const pugi::xml_node &in, Context &context);
void html(const pugi::xml_node &in, Context &context);
} // namespace workbook_translator

} // namespace odr::internal::ooxml

#endif // ODR_INTERNAL_OOXML_WORKBOOK_TRANSLATOR_H
