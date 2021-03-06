#include <internal/abstract/document.h>
#include <internal/abstract/document_elements.h>
#include <internal/common/path.h>
#include <odr/document.h>
#include <odr/document_elements.h>
#include <odr/document_meta.h>
#include <odr/document_style.h>

namespace odr {

Document::Document(std::shared_ptr<internal::abstract::Document> document)
    : m_document{std::move(document)} {
  if (!m_document) {
    throw std::runtime_error("document is null");
  }
}

DocumentType Document::document_type() const noexcept {
  return m_document->document_type();
}

DocumentMeta Document::document_meta() const noexcept {
  return m_document->document_meta();
}

bool Document::editable() const noexcept { return m_document->editable(); }

bool Document::savable(const bool encrypted) const noexcept {
  return m_document->savable(encrypted);
}

TextDocument Document::text_document() const {
  return TextDocument(
      std::dynamic_pointer_cast<internal::abstract::TextDocument>(m_document));
}

Presentation Document::presentation() const {
  return Presentation(
      std::dynamic_pointer_cast<internal::abstract::Presentation>(m_document));
}

Spreadsheet Document::spreadsheet() const {
  return Spreadsheet(
      std::dynamic_pointer_cast<internal::abstract::Spreadsheet>(m_document));
}

Drawing Document::drawing() const {
  return Drawing(
      std::dynamic_pointer_cast<internal::abstract::Drawing>(m_document));
}

Element Document::root() const { return Element(m_document->root()); }

void Document::save(const std::string &path) const { m_document->save(path); }

void Document::save(const std::string &path,
                    const std::string &password) const {
  m_document->save(path, password);
}

TextDocument::TextDocument(
    std::shared_ptr<internal::abstract::TextDocument> textDocument)
    : Document(textDocument), m_text_document{std::move(textDocument)} {}

PageStyle TextDocument::page_style() const {
  return PageStyle(m_text_document->page_style());
}

ElementRange TextDocument::content() const { return root().children(); }

Presentation::Presentation(
    std::shared_ptr<internal::abstract::Presentation> presentation)
    : Document(presentation), m_presentation{std::move(presentation)} {}

std::uint32_t Presentation::slide_count() const {
  return m_presentation->slide_count();
}

SlideRange Presentation::slides() const {
  return SlideRange(SlideElement(m_presentation->first_slide()));
}

Spreadsheet::Spreadsheet(
    std::shared_ptr<internal::abstract::Spreadsheet> spreadsheet)
    : Document(spreadsheet), m_spreadsheet{std::move(spreadsheet)} {}

std::uint32_t Spreadsheet::sheet_count() const {
  return m_spreadsheet->sheet_count();
}

SheetRange Spreadsheet::sheets() const {
  return SheetRange(SheetElement(m_spreadsheet->first_sheet()));
}

Drawing::Drawing(std::shared_ptr<internal::abstract::Drawing> graphics)
    : Document(graphics), m_drawing{std::move(graphics)} {}

std::uint32_t Drawing::page_count() const { return m_drawing->page_count(); }

PageRange Drawing::pages() const {
  return PageRange(PageElement(m_drawing->first_page()));
}

} // namespace odr
