#ifndef ODR_OOXML_WORKBOOK_TRANSLATOR_H
#define ODR_OOXML_WORKBOOK_TRANSLATOR_H

#include <memory>

namespace pugi {
class xml_node;
}

namespace odr {
namespace ooxml {

struct Context;

namespace WorkbookTranslator {
void css(const pugi::xml_node &in, Context &context);
void html(const pugi::xml_node &in, Context &context);
} // namespace WorkbookTranslator

} // namespace ooxml
} // namespace odr

#endif // ODR_OOXML_WORKBOOK_TRANSLATOR_H
