#include <internal/abstract/document.h>
#include <odr/experimental/document_type.h>

namespace odr::internal::abstract {

experimental::DocumentType TextDocument::document_type() const noexcept {
  return experimental::DocumentType::TEXT;
}

experimental::DocumentType Presentation::document_type() const noexcept {
  return experimental::DocumentType::PRESENTATION;
}

experimental::DocumentType Spreadsheet::document_type() const noexcept {
  return experimental::DocumentType::SPREADSHEET;
}

experimental::DocumentType Drawing::document_type() const noexcept {
  return experimental::DocumentType::DRAWING;
}

} // namespace odr::internal::abstract
