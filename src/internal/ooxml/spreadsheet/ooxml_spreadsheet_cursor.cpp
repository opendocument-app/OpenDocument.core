#include <internal/abstract/document.h>
#include <internal/common/document_cursor.h>
#include <internal/ooxml/spreadsheet/ooxml_spreadsheet_cursor.h>
#include <internal/ooxml/spreadsheet/ooxml_spreadsheet_document.h>
#include <internal/ooxml/spreadsheet/ooxml_spreadsheet_element.h>
#include <pugixml.hpp>
#include <stdexcept>

namespace odr::internal::ooxml::spreadsheet {

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

} // namespace odr::internal::ooxml::spreadsheet
