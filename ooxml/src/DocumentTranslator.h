#ifndef ODR_OOXML_DOCUMENT_TRANSLATOR_H
#define ODR_OOXML_DOCUMENT_TRANSLATOR_H

#include <memory>

namespace pugi {
class xml_node;
}

namespace odr::ooxml {

struct Context;

namespace DocumentTranslator {
void css(const pugi::xml_node &in, Context &context);
void html(const pugi::xml_node &in, Context &context);
} // namespace DocumentTranslator

} // namespace odr::ooxml

#endif // ODR_OOXML_DOCUMENT_TRANSLATOR_H
