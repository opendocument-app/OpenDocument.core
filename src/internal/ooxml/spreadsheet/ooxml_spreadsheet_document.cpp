#include <internal/common/path.h>
#include <internal/ooxml/ooxml_util.h>
#include <internal/ooxml/spreadsheet/ooxml_spreadsheet_cursor.h>
#include <internal/ooxml/spreadsheet/ooxml_spreadsheet_document.h>
#include <internal/util/xml_util.h>
#include <odr/exceptions.h>
#include <odr/file.h>
#include <pugixml.hpp>
#include <unordered_map>
#include <utility>

namespace odr::internal::abstract {
class DocumentCursor;
class ReadableFilesystem;
} // namespace odr::internal::abstract

namespace odr::internal::ooxml::spreadsheet {

Document::Document(std::shared_ptr<abstract::ReadableFilesystem> filesystem)
    : m_filesystem{std::move(filesystem)} {
  m_workbook_xml = util::xml::parse(*m_filesystem, "xl/workbook.xml");
  m_styles_xml = util::xml::parse(*m_filesystem, "xl/styles.xml");

  for (auto relationships :
       parse_relationships(*m_filesystem, "xl/workbook.xml")) {
    m_sheets_xml[relationships.first] = util::xml::parse(
        *m_filesystem, common::Path("xl").join(relationships.second));
  }
}

bool Document::editable() const noexcept { return false; }

bool Document::savable(const bool /*encrypted*/) const noexcept {
  return false;
}

void Document::save(const common::Path & /*path*/) const {
  throw UnsupportedOperation();
}

void Document::save(const common::Path & /*path*/,
                    const char * /*password*/) const {
  throw UnsupportedOperation();
}

DocumentType Document::document_type() const noexcept {
  return DocumentType::spreadsheet;
}

std::shared_ptr<abstract::ReadableFilesystem> Document::files() const noexcept {
  return m_filesystem;
}

std::unique_ptr<abstract::DocumentCursor> Document::root_element() const {
  return std::make_unique<DocumentCursor>(this,
                                          m_workbook_xml.document_element());
}

} // namespace odr::internal::ooxml::spreadsheet
