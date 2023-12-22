#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_document.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/ooxml/ooxml_util.hpp>
#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_parser.hpp>
#include <odr/internal/util/xml_util.hpp>

#include <utility>

namespace odr::internal::ooxml::spreadsheet {

Document::Document(std::shared_ptr<abstract::ReadableFilesystem> filesystem)
    : common::TemplateDocument<Element>(FileType::office_open_xml_workbook,
                                        DocumentType::spreadsheet,
                                        std::move(filesystem)) {
  m_workbook_xml = util::xml::parse(*m_filesystem, "xl/workbook.xml");
  m_styles_xml = util::xml::parse(*m_filesystem, "xl/styles.xml");

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

  m_root_element = parse_tree(*this, m_workbook_xml.document_element());

  m_style_registry = StyleRegistry(m_styles_xml.document_element());

  if (m_filesystem->exists("xl/sharedStrings.xml")) {
    m_shared_strings_xml =
        util::xml::parse(*m_filesystem, "xl/sharedStrings.xml");
  }

  for (auto shared_string : m_shared_strings_xml.document_element()) {
    m_shared_strings.push_back(shared_string);
  }
}

bool Document::is_editable() const noexcept { return false; }

bool Document::is_savable(const bool /*encrypted*/) const noexcept {
  return false;
}

void Document::save(const common::Path & /*path*/) const {
  throw UnsupportedOperation();
}

void Document::save(const common::Path & /*path*/,
                    const char * /*password*/) const {
  throw UnsupportedOperation();
}

pugi::xml_node Document::get_sheet_root(const std::string &ref) const {
  if (auto it = m_sheets.find(ref); it != std::end(m_sheets)) {
    return it->second.sheet_xml.document_element();
  }
  return {};
}

} // namespace odr::internal::ooxml::spreadsheet
