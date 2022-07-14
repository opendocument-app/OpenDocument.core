#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/ooxml/ooxml_util.hpp>
#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_cursor.hpp>
#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_document.hpp>
#include <odr/internal/util/xml_util.hpp>
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

  m_style_registry = StyleRegistry(m_styles_xml.document_element());

  for (const auto &relationships :
       parse_relationships(*m_filesystem, "xl/workbook.xml")) {
    auto sheet_path = common::Path("xl").join(relationships.second);
    auto sheet_xml = util::xml::parse(*m_filesystem, sheet_path);

    if (auto drawing = sheet_xml.document_element().child("drawing")) {
      auto sheet_relationships = parse_relationships(*m_filesystem, sheet_path);
      auto drawing_path =
          common::Path("xl/worksheets")
              .join(sheet_relationships.at(drawing.attribute("r:id").value()));
      auto drawing_xml = util::xml::parse(*m_filesystem, drawing_path);

      m_sheets[relationships.first].sheet_path = std::move(drawing_path);
      m_sheets[relationships.first].drawing_xml = std::move(drawing_xml);
    }

    m_sheets[relationships.first].sheet_path = std::move(sheet_path);
    m_sheets[relationships.first].sheet_xml = std::move(sheet_xml);
  }

  if (m_filesystem->exists("xl/sharedStrings.xml")) {
    m_shared_strings_xml =
        util::xml::parse(*m_filesystem, "xl/sharedStrings.xml");
  }

  for (auto shared_string : m_shared_strings_xml.document_element()) {
    m_shared_strings.push_back(shared_string);
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

FileType Document::file_type() const noexcept {
  return FileType::office_open_xml_workbook;
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
