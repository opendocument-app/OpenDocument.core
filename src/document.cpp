#include <access/path.h>
#include <common/document.h>
#include <common/document_elements.h>
#include <odr/document.h>
#include <odr/document_elements.h>
#include <odr/document_style.h>

namespace odr {

Document::Document(std::shared_ptr<common::Document> document)
    : m_document{std::move(document)} {
  // TODO null check
}

DocumentType Document::documentType() const noexcept {
  return m_document->documentType();
}

DocumentMeta Document::documentMeta() const noexcept {
  return m_document->documentMeta();
}

bool Document::editable() const noexcept { return m_document->editable(); }

bool Document::savable(const bool encrypted) const noexcept {
  return m_document->savable(encrypted);
}

TextDocument Document::textDocument() const {
  return TextDocument(
      std::dynamic_pointer_cast<common::TextDocument>(m_document));
}

Presentation Document::presentation() const {
  return Presentation(
      std::dynamic_pointer_cast<common::Presentation>(m_document));
}

Spreadsheet Document::spreadsheet() const {
  return Spreadsheet(
      std::dynamic_pointer_cast<common::Spreadsheet>(m_document));
}

Drawing Document::drawing() const {
  return Drawing(std::dynamic_pointer_cast<common::Drawing>(m_document));
}

Element Document::root() const { return Element(m_document->root()); }

void Document::save(const std::string &path) const { m_document->save(path); }

void Document::save(const std::string &path,
                    const std::string &password) const {
  m_document->save(path, password);
}

TextDocument::TextDocument(std::shared_ptr<common::TextDocument> textDocument)
    : Document(textDocument), m_textDocument{std::move(textDocument)} {}

PageStyle TextDocument::pageStyle() const {
  return PageStyle(m_textDocument->pageStyle());
}

ElementRange TextDocument::content() const { return root().children(); }

Presentation::Presentation(std::shared_ptr<common::Presentation> presentation)
    : Document(presentation), m_presentation{std::move(presentation)} {}

std::uint32_t Presentation::slideCount() const {
  return m_presentation->slideCount();
}

SlideRange Presentation::slides() const {
  return SlideRange(SlideElement(m_presentation->firstSlide()));
}

Spreadsheet::Spreadsheet(std::shared_ptr<common::Spreadsheet> spreadsheet)
    : Document(spreadsheet), m_spreadsheet{std::move(spreadsheet)} {}

std::uint32_t Spreadsheet::sheetCount() const {
  return m_spreadsheet->sheetCount();
}

SheetRange Spreadsheet::sheets() const {
  return SheetRange(SheetElement(m_spreadsheet->firstSheet()));
}

Drawing::Drawing(std::shared_ptr<common::Drawing> graphics)
    : Document(graphics), m_drawing{std::move(graphics)} {}

std::uint32_t Drawing::pageCount() const { return m_drawing->pageCount(); }

PageRange Drawing::pages() const {
  return PageRange(PageElement(m_drawing->firstPage()));
}

} // namespace odr
