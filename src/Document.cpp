#include <access/Path.h>
#include <common/Document.h>
#include <common/DocumentElements.h>
#include <odr/Document.h>
#include <odr/DocumentElements.h>

namespace odr {

Document::Document(std::shared_ptr<common::Document> document)
    : m_document{std::move(document)} {}

DocumentType Document::documentType() const noexcept {
  return {}; // TODO
}

bool Document::editable() const noexcept { return m_document->editable(); }

bool Document::savable(const bool encrypted) const noexcept {
  return m_document->savable(encrypted);
}

TextDocument Document::textDocument() const { return TextDocument(*this); }

Presentation Document::presentation() const { return Presentation(*this); }

Spreadsheet Document::spreadsheet() const { return Spreadsheet(*this); }

Graphics Document::graphics() const { return Graphics(*this); }

void Document::save(const std::string &path) const { m_document->save(path); }

void Document::save(const std::string &path,
                    const std::string &password) const {
  m_document->save(path, password);
}

TextDocument::TextDocument(const Document &document)
    : Document(document), m_text_document{
                              std::dynamic_pointer_cast<common::TextDocument>(
                                  m_document)} {
  // TODO throw if nullptr
}

PageProperties TextDocument::pageProperties() const {
  return m_text_document->pageProperties();
}

ElementSiblingRange TextDocument::content() const {
  return m_text_document->content();
}

Presentation::Presentation(const Document &document)
    : Document(document), m_presentation{
                              std::dynamic_pointer_cast<common::Presentation>(
                                  m_document)} {
  // TODO throw if nullptr
}

Spreadsheet::Spreadsheet(const Document &document)
    : Document(document), m_spreadsheet{
                              std::dynamic_pointer_cast<common::Spreadsheet>(
                                  m_document)} {
  // TODO throw if nullptr
}

Graphics::Graphics(const Document &document)
    : Document(document),
      m_graphics{std::dynamic_pointer_cast<common::Graphics>(m_document)} {
  // TODO throw if nullptr
}

} // namespace odr
