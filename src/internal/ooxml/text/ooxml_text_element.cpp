#include <internal/ooxml/text/ooxml_text_document.h>
#include <internal/ooxml/text/ooxml_text_element.h>

namespace odr::internal::ooxml::text {}

namespace odr::internal::ooxml {

abstract::Element *text::construct_default_element(const Document *document,
                                                   pugi::xml_node node,
                                                   const Allocator &allocator) {
  return nullptr;
}

} // namespace odr::internal::ooxml
