#include <internal/odf/odf_cursor.h>
#include <internal/odf/odf_document.h>
#include <internal/odf/odf_element.h>

namespace odr::internal::odf {

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

common::ResolvedStyle DocumentCursor::partial_style() const {
  return dynamic_cast<const odf::Element *>(back_())->partial_style(m_document);
}

} // namespace odr::internal::odf
