#include <common/path.h>
#include <odr/document.h>
#include <odr/document_elements.h>
#include <ooxml/ooxml_document.h>
#include <ooxml/text/ooxml_text_elements.h>
#include <util/xml_util.h>
#include <zip/zip_storage.h>

namespace odr::ooxml {

OfficeOpenXmlDocument::OfficeOpenXmlDocument(
    std::shared_ptr<abstract::ReadStorage> storage)
    : m_storage{std::move(storage)} {}

bool OfficeOpenXmlDocument::editable() const noexcept { return true; }

bool OfficeOpenXmlDocument::savable(bool encrypted) const noexcept {
  return false;
}

DocumentType OfficeOpenXmlDocument::documentType() const noexcept {
  return documentMeta().documentType;
}

std::shared_ptr<abstract::ReadStorage>
OfficeOpenXmlDocument::storage() const noexcept {
  return m_storage;
}

void OfficeOpenXmlDocument::save(const common::Path &path) const {
  // TODO
}

void OfficeOpenXmlDocument::save(const common::Path &path,
                                 const std::string &password) const {
  // TODO
}

OfficeOpenXmlTextDocument::OfficeOpenXmlTextDocument(
    std::shared_ptr<abstract::ReadStorage> storage)
    : OfficeOpenXmlDocument(std::move(storage)) {
  m_documentXml = util::xml::parse(*m_storage, "word/document.xml");
  m_stylesXml = util::xml::parse(*m_storage, "word/styles.xml");

  m_styles = text::Styles(m_stylesXml.document_element(),
                          m_documentXml.document_element());
}

DocumentMeta OfficeOpenXmlTextDocument::documentMeta() const noexcept {
  DocumentMeta result;

  result.documentType = DocumentType::TEXT;

  return result;
}

const text::Styles &OfficeOpenXmlTextDocument::styles() const noexcept {
  return m_styles;
}

std::shared_ptr<const abstract::Element>
OfficeOpenXmlTextDocument::root() const {
  const pugi::xml_node body = m_documentXml.document_element().child("w:body");
  return text::factorizeRoot(shared_from_this(), body);
}

std::shared_ptr<abstract::PageStyle>
OfficeOpenXmlTextDocument::pageStyle() const {
  return m_styles.pageStyle();
}

OfficeOpenXmlPresentation::OfficeOpenXmlPresentation(
    std::shared_ptr<abstract::ReadStorage> storage)
    : OfficeOpenXmlDocument(std::move(storage)) {
  m_presentationXml = util::xml::parse(*m_storage, "ppt/presentation.xml");
}

DocumentMeta OfficeOpenXmlPresentation::documentMeta() const noexcept {
  DocumentMeta result;

  result.documentType = DocumentType::PRESENTATION;
  result.entryCount = 0;
  for (auto &&e : m_presentationXml.select_nodes("//p:sldId")) {
    ++result.entryCount;
    DocumentMeta::Entry entry;
    // TODO
    result.entries.emplace_back(entry);
  }

  return result;
}

std::uint32_t OfficeOpenXmlPresentation::slideCount() const {
  return 0; // TODO
}

std::shared_ptr<const abstract::Element>
OfficeOpenXmlPresentation::root() const {
  return {}; // TODO
}

std::shared_ptr<const abstract::Slide>
OfficeOpenXmlPresentation::firstSlide() const {
  return std::dynamic_pointer_cast<const abstract::Slide>(root()->firstChild());
}

OfficeOpenXmlSpreadsheet::OfficeOpenXmlSpreadsheet(
    std::shared_ptr<abstract::ReadStorage> storage)
    : OfficeOpenXmlDocument(std::move(storage)) {
  m_workbookXml = util::xml::parse(*m_storage, "xl/workbook.xml");
  m_stylesXml = util::xml::parse(*m_storage, "xl/styles.xml");
}

DocumentMeta OfficeOpenXmlSpreadsheet::documentMeta() const noexcept {
  DocumentMeta result;

  result.documentType = DocumentType::SPREADSHEET;
  result.entryCount = 0;
  for (auto &&e : m_workbookXml.select_nodes("//sheet")) {
    ++result.entryCount;
    DocumentMeta::Entry entry;
    entry.name = e.node().attribute("name").as_string();
    // TODO dimension
    result.entries.emplace_back(entry);
  }

  return result;
}

std::uint32_t OfficeOpenXmlSpreadsheet::sheetCount() const {
  return 0; // TODO
}

std::shared_ptr<const abstract::Element>
OfficeOpenXmlSpreadsheet::root() const {
  return {}; // TODO
}

std::shared_ptr<const abstract::Sheet>
OfficeOpenXmlSpreadsheet::firstSheet() const {
  return std::dynamic_pointer_cast<const abstract::Sheet>(root()->firstChild());
}

} // namespace odr::ooxml
