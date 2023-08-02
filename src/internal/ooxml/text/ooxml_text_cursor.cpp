#include <internal/abstract/document.h>
#include <internal/common/document_cursor.h>
#include <internal/ooxml/text/ooxml_text_cursor.h>
#include <internal/ooxml/text/ooxml_text_document.h>
#include <internal/ooxml/text/ooxml_text_element.h>
#include <pugixml.hpp>
#include <stdexcept>

namespace odr::internal::ooxml::text {

DocumentCursor::DocumentCursor(const Document *document, pugi::xml_node root)
    : common::DocumentCursor(document) {
  auto element = Element::construct_default_element(root);
  if (!element) {
    throw std::invalid_argument("root element invalid");
  }
  push_element_(std::move(element));
  push_style_(document->m_style_registry.default_style()->resolved());
}

[[nodiscard]] std::unique_ptr<abstract::DocumentCursor>
DocumentCursor::copy() const {
  return std::make_unique<DocumentCursor>(*this);
}

common::ResolvedStyle DocumentCursor::partial_style() const {
  return dynamic_cast<const Element *>(element())->partial_style(m_document);
}

} // namespace odr::internal::ooxml::text
