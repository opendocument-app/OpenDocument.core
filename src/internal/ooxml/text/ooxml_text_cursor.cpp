#include <internal/ooxml/text/ooxml_text_cursor.h>
#include <internal/ooxml/text/ooxml_text_document.h>
#include <internal/ooxml/text/ooxml_text_element.h>

namespace odr::internal::ooxml::text {

DocumentCursor::DocumentCursor(const Document *document, pugi::xml_node root)
    : common::DocumentCursor(document) {
  abstract::Allocator allocator = [this](std::size_t size) {
    return push_(size);
  };
  auto element = construct_default_element(document, root, &allocator);
  if (!element) {
    throw std::invalid_argument("root element invalid");
  }
}

} // namespace odr::internal::ooxml::text
