#ifndef ODR_OOXML_DOCUMENT_TRANSLATOR_H
#define ODR_OOXML_DOCUMENT_TRANSLATOR_H

#include <memory>

namespace tinyxml2 {
class XMLElement;
}

namespace odr {
namespace ooxml {

struct Context;

namespace DocumentTranslator {
void css(const tinyxml2::XMLElement &in, Context &context);
void html(const tinyxml2::XMLElement &in, Context &context);
} // namespace DocumentTranslator

} // namespace ooxml
} // namespace odr

#endif // ODR_OOXML_DOCUMENT_TRANSLATOR_H
