#include <abstract/document.h>
#include <odr/document.h>

namespace odr::abstract {

DocumentType TextDocument::document_type() const noexcept {
  return DocumentType::TEXT;
}

DocumentType Presentation::document_type() const noexcept {
  return DocumentType::PRESENTATION;
}

DocumentType Spreadsheet::document_type() const noexcept {
  return DocumentType::SPREADSHEET;
}

DocumentType Drawing::document_type() const noexcept {
  return DocumentType::DRAWING;
}

} // namespace odr::abstract
