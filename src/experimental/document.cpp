#include <internal/abstract/document.h>
#include <internal/common/path.h>
#include <odr/experimental/document.h>
#include <odr/experimental/document_elements.h>
#include <odr/experimental/document_meta.h>
#include <odr/experimental/document_style.h>

namespace odr::experimental {

Document::Document(std::shared_ptr<internal::abstract::Document> document)
    : m_document{std::move(document)} {
  if (!m_document) {
    throw std::runtime_error("document is null");
  }
}

DocumentType Document::document_type() const noexcept {
  return m_document->document_type();
}

bool Document::editable() const noexcept { return m_document->editable(); }

bool Document::savable(const bool encrypted) const noexcept {
  return m_document->savable(encrypted);
}

TextDocument Document::text_document() const {
  return TextDocument(m_document);
}

Presentation Document::presentation() const { return Presentation(m_document); }

Spreadsheet Document::spreadsheet() const { return Spreadsheet(m_document); }

Drawing Document::drawing() const { return Drawing(m_document); }

Element Document::root() const { return Element(m_document->root()); }

void Document::save(const std::string &path) const { m_document->save(path); }

void Document::save(const std::string &path,
                    const std::string &password) const {
  m_document->save(path, password.c_str());
}

TextDocument::TextDocument(
    std::shared_ptr<internal::abstract::Document> text_document)
    : Document(std::move(text_document)) {
  if (m_document->document_type() != DocumentType::TEXT) {
    throw std::runtime_error("not a text document");
  }
}

PageStyle TextDocument::page_style() const {
  return PageStyle(m_document->page_style());
}

ElementRange TextDocument::content() const { return root().children(); }

Presentation::Presentation(
    std::shared_ptr<internal::abstract::Document> presentation)
    : Document(std::move(presentation)) {
  if (m_document->document_type() != DocumentType::PRESENTATION) {
    throw std::runtime_error("not a presentation");
  }
}

std::uint32_t Presentation::slide_count() const {
  return m_document->slide_count();
}

SlideRange Presentation::slides() const {
  return SlideRange(SlideElement(m_document->first_slide()));
}

Spreadsheet::Spreadsheet(
    std::shared_ptr<internal::abstract::Document> spreadsheet)
    : Document(std::move(spreadsheet)) {
  if (m_document->document_type() != DocumentType::SPREADSHEET) {
    throw std::runtime_error("not a spreadsheet");
  }
}

std::uint32_t Spreadsheet::sheet_count() const {
  return m_document->sheet_count();
}

SheetRange Spreadsheet::sheets() const {
  return SheetRange(SheetElement(m_document->first_sheet()));
}

Drawing::Drawing(std::shared_ptr<internal::abstract::Document> drawing)
    : Document(std::move(drawing)) {
  if (m_document->document_type() != DocumentType::DRAWING) {
    throw std::runtime_error("not a drawing");
  }
}

std::uint32_t Drawing::page_count() const { return m_document->page_count(); }

PageRange Drawing::pages() const {
  return PageRange(PageElement(m_document->first_page()));
}

} // namespace odr::experimental
