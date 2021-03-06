#include <internal/common/path.h>
#include <internal/ooxml/ooxml_document.h>
#include <internal/ooxml/text/ooxml_text_elements.h>
#include <internal/util/xml_util.h>
#include <odr/document.h>
#include <odr/document_elements.h>
#include <odr/exceptions.h>

namespace odr::internal::ooxml {

OfficeOpenXmlDocument::OfficeOpenXmlDocument(
    std::shared_ptr<abstract::ReadableFilesystem> filesystem)
    : m_filesystem{std::move(filesystem)} {}

bool OfficeOpenXmlDocument::editable() const noexcept { return true; }

bool OfficeOpenXmlDocument::savable(bool encrypted) const noexcept {
  return false;
}

std::shared_ptr<abstract::ReadableFilesystem>
OfficeOpenXmlDocument::filesystem() const noexcept {
  return m_filesystem;
}

void OfficeOpenXmlDocument::save(const common::Path &path) const {
  throw UnsupportedOperation();
}

void OfficeOpenXmlDocument::save(const common::Path &path,
                                 const std::string &password) const {
  throw UnsupportedOperation();
}

OfficeOpenXmlTextDocument::OfficeOpenXmlTextDocument(
    std::shared_ptr<abstract::ReadableFilesystem> filesystem)
    : OfficeOpenXmlDocument(std::move(filesystem)) {
  m_document_xml = util::xml::parse(*m_filesystem, "word/document.xml");
  m_styles_xml = util::xml::parse(*m_filesystem, "word/styles.xml");

  m_styles = text::Styles(m_styles_xml.document_element(),
                          m_document_xml.document_element());
}

DocumentMeta OfficeOpenXmlTextDocument::document_meta() const noexcept {
  DocumentMeta result;

  result.document_type = document_type();

  return result;
}

const text::Styles &OfficeOpenXmlTextDocument::styles() const noexcept {
  return m_styles;
}

std::shared_ptr<const abstract::Element>
OfficeOpenXmlTextDocument::root() const {
  const pugi::xml_node body = m_document_xml.document_element().child("w:body");
  return text::factorize_root(shared_from_this(), body);
}

std::shared_ptr<abstract::PageStyle>
OfficeOpenXmlTextDocument::page_style() const {
  return m_styles.page_style();
}

OfficeOpenXmlPresentation::OfficeOpenXmlPresentation(
    std::shared_ptr<abstract::ReadableFilesystem> filesystem)
    : OfficeOpenXmlDocument(std::move(filesystem)) {
  m_presentation_xml = util::xml::parse(*m_filesystem, "ppt/presentation.xml");
}

DocumentMeta OfficeOpenXmlPresentation::document_meta() const noexcept {
  DocumentMeta result;

  result.document_type = document_type();

  result.entry_count = 0;
  for (auto &&e : m_presentation_xml.select_nodes("//p:sldId")) {
    ++result.entry_count;
    DocumentMeta::Entry entry;
    // TODO
    result.entries.emplace_back(entry);
  }

  return result;
}

std::uint32_t OfficeOpenXmlPresentation::slide_count() const {
  return document_meta().entry_count;
}

std::shared_ptr<const abstract::Element>
OfficeOpenXmlPresentation::root() const {
  return {}; // TODO
}

std::shared_ptr<const abstract::Slide>
OfficeOpenXmlPresentation::first_slide() const {
  return std::dynamic_pointer_cast<const abstract::Slide>(
      root()->first_child());
}

OfficeOpenXmlSpreadsheet::OfficeOpenXmlSpreadsheet(
    std::shared_ptr<abstract::ReadableFilesystem> filesystem)
    : OfficeOpenXmlDocument(std::move(filesystem)) {
  m_workbook_xml = util::xml::parse(*m_filesystem, "xl/workbook.xml");
  m_styles_xml = util::xml::parse(*m_filesystem, "xl/styles.xml");
}

DocumentMeta OfficeOpenXmlSpreadsheet::document_meta() const noexcept {
  DocumentMeta result;

  result.document_type = document_type();

  result.entry_count = 0;
  for (auto &&e : m_workbook_xml.select_nodes("//sheet")) {
    ++result.entry_count;
    DocumentMeta::Entry entry;
    entry.name = e.node().attribute("name").as_string();
    // TODO dimension
    result.entries.emplace_back(entry);
  }

  return result;
}

std::uint32_t OfficeOpenXmlSpreadsheet::sheet_count() const {
  return document_meta().entry_count;
}

std::shared_ptr<const abstract::Element>
OfficeOpenXmlSpreadsheet::root() const {
  return {}; // TODO
}

std::shared_ptr<const abstract::Sheet>
OfficeOpenXmlSpreadsheet::first_sheet() const {
  return std::dynamic_pointer_cast<const abstract::Sheet>(
      root()->first_child());
}

} // namespace odr::internal::ooxml
