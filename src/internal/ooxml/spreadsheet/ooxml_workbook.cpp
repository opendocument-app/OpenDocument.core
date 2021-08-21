#include <internal/common/path.h>
#include <internal/ooxml/ooxml_util.h>
#include <internal/ooxml/spreadsheet/ooxml_workbook.h>
#include <internal/util/xml_util.h>
#include <odr/document.h>
#include <odr/exceptions.h>
#include <odr/file.h>

namespace odr::internal::ooxml {

OfficeOpenXmlWorkbook::OfficeOpenXmlWorkbook(
    std::shared_ptr<abstract::ReadableFilesystem> filesystem)
    : m_filesystem{std::move(filesystem)} {
  auto workbook_xml = util::xml::parse(*m_filesystem, "xl/workbook.xml");
  m_styles_xml = util::xml::parse(*m_filesystem, "xl/styles.xml");

  // TODO root
}

bool OfficeOpenXmlWorkbook::editable() const noexcept { return false; }

bool OfficeOpenXmlWorkbook::savable(const bool /*encrypted*/) const noexcept {
  return false;
}

void OfficeOpenXmlWorkbook::save(const common::Path & /*path*/) const {
  throw UnsupportedOperation();
}

void OfficeOpenXmlWorkbook::save(const common::Path & /*path*/,
                                 const char * /*password*/) const {
  throw UnsupportedOperation();
}

DocumentType OfficeOpenXmlWorkbook::document_type() const noexcept {
  return DocumentType::PRESENTATION;
}

std::shared_ptr<abstract::ReadableFilesystem>
OfficeOpenXmlWorkbook::files() const noexcept {
  return m_filesystem;
}

odr::Element OfficeOpenXmlWorkbook::root_element() const { return m_root; }

} // namespace odr::internal::ooxml
