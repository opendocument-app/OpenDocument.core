#include <odr/internal/abstract/document.hpp>
#include <odr/internal/common/document_cursor.hpp>
#include <odr/internal/odf/odf_cursor.hpp>
#include <odr/internal/odf/odf_document.hpp>
#include <odr/internal/odf/odf_element.hpp>
#include <pugixml.hpp>
#include <stdexcept>

namespace odr::internal::odf {

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

common::ResolvedStyle DocumentCursor::partial_style() const {
  return dynamic_cast<const Element *>(element())->partial_style(m_document);
}

} // namespace odr::internal::odf
