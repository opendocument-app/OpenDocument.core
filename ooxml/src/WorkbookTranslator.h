#ifndef ODR_OOXML_WORKBOOK_TRANSLATOR_H
#define ODR_OOXML_WORKBOOK_TRANSLATOR_H

#include <memory>

namespace pugi {
class xml_node;
}

namespace odr::ooxml {

struct Context;

namespace WorkbookTranslator {
void css(pugi::xml_node in, Context &context);
void html(pugi::xml_node in, Context &context);
} // namespace WorkbookTranslator

} // namespace odr::ooxml

#endif // ODR_OOXML_WORKBOOK_TRANSLATOR_H
