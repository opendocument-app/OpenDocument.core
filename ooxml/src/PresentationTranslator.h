#ifndef ODR_OOXML_PRESENTATION_TRANSLATOR_H
#define ODR_OOXML_PRESENTATION_TRANSLATOR_H

#include <memory>

namespace tinyxml2 {
class XMLElement;
}

namespace odr {
namespace ooxml {

struct Context;

namespace PresentationTranslator {
void css(const tinyxml2::XMLElement &in, Context &context);
void html(const tinyxml2::XMLElement &in, Context &context);
} // namespace PresentationTranslator

} // namespace ooxml
} // namespace odr

#endif // ODR_OOXML_PRESENTATION_TRANSLATOR_H
