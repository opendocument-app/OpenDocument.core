#include <internal/abstract/document.h>
#include <internal/common/document_cursor.h>
#include <internal/ooxml/presentation/ooxml_presentation_cursor.h>
#include <internal/ooxml/presentation/ooxml_presentation_document.h>
#include <internal/ooxml/presentation/ooxml_presentation_element.h>
#include <pugixml.hpp>
#include <stdexcept>

namespace odr::internal::ooxml::presentation {

DocumentCursor::DocumentCursor(const Document *document, pugi::xml_node root)
    : common::DocumentCursor(document) {
  auto element = Element::construct_default_element(root);
  if (!element) {
    throw std::invalid_argument("root element invalid");
  }
  push_element_(std::move(element));
}

[[nodiscard]] std::unique_ptr<abstract::DocumentCursor>
DocumentCursor::copy() const {
  return std::make_unique<DocumentCursor>(*this);
}

} // namespace odr::internal::ooxml::presentation
