#include <internal/ooxml/presentation/ooxml_presentation_cursor.h>
#include <internal/ooxml/presentation/ooxml_presentation_document.h>
#include <internal/ooxml/presentation/ooxml_presentation_element.h>

namespace odr::internal::ooxml::presentation {

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

[[nodiscard]] std::unique_ptr<abstract::DocumentCursor>
DocumentCursor::copy() const {
  return std::make_unique<DocumentCursor>(*this);
}

} // namespace odr::internal::ooxml::presentation
