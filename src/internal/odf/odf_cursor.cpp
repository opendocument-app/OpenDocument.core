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
  pushed_(element);
}

const ResolvedStyle &DocumentCursor::current_style() const {
  return m_style_stack.back();
}

void DocumentCursor::pushed_(abstract::Element *element) {
  if (m_style_stack.empty()) {
    m_style_stack.emplace_back();
  } else {
    m_style_stack.emplace_back(m_style_stack.back());
  }
  m_style_stack.back().override(
      dynamic_cast<odf::Element *>(element)->element_style(m_document));
}

void DocumentCursor::popping_(abstract::Element *) { m_style_stack.pop_back(); }

} // namespace odr::internal::odf
