#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_document.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_parser.hpp>
#include <odr/internal/util/xml_util.hpp>

#include <utility>

namespace odr::internal::ooxml::spreadsheet {

Document::Document(std::shared_ptr<abstract::ReadableFilesystem> filesystem)
    : TemplateDocument<Element>(FileType::office_open_xml_workbook,
                                DocumentType::spreadsheet,
                                std::move(filesystem)) {
  auto workbook_path = Path("/xl/workbook.xml");
  auto [workbook_xml, workbook_relations] = parse_xml_(workbook_path);
  auto [styles_xml, _] = parse_xml_(Path("/xl/styles.xml"));

  for (pugi::xml_node sheet_node :
       workbook_xml.document_element().child("sheets").children("sheet")) {
    const char *id = sheet_node.attribute("r:id").value();
    Path sheet_path =
        workbook_path.parent().join(Path(workbook_relations.at(id)));
    auto [sheet_xml, sheet_relationships] = parse_xml_(sheet_path);

    if (auto drawing = sheet_xml.document_element().child("drawing")) {
      auto drawing_path = sheet_path.parent().join(
          Path(sheet_relationships.at(drawing.attribute("r:id").value())));
      parse_xml_(drawing_path);
    }
  }

  if (m_filesystem->exists(Path("/xl/sharedStrings.xml"))) {
    auto [shared_strings_xml, _] = parse_xml_(Path("/xl/sharedStrings.xml"));

    for (auto shared_string : shared_strings_xml.document_element()) {
      m_shared_strings.push_back(shared_string);
    }
  }

  m_style_registry = StyleRegistry(styles_xml.document_element());

  m_root_element = parse_tree(*this, workbook_xml.document_element(),
                              workbook_path, workbook_relations);
}

std::pair<pugi::xml_document &, Relations &>
Document::parse_xml_(const Path &path) {
  pugi::xml_document document = util::xml::parse(*m_filesystem, path);
  Relations relations = parse_relationships(*m_filesystem, path);

  auto result = m_xml.emplace(
      path, std::make_pair(std::move(document), std::move(relations)));
  return {result.first->second.first, result.first->second.second};
}

bool Document::is_editable() const noexcept { return false; }

bool Document::is_savable(const bool /*encrypted*/) const noexcept {
  return false;
}

void Document::save(const Path & /*path*/) const {
  throw UnsupportedOperation();
}

void Document::save(const Path & /*path*/, const char * /*password*/) const {
  throw UnsupportedOperation();
}

std::pair<const pugi::xml_document &, const Relations &>
Document::get_xml(const Path &path) const {
  return m_xml.at(path);
}

pugi::xml_node Document::get_shared_string(std::size_t index) const {
  return m_shared_strings.at(index);
}

} // namespace odr::internal::ooxml::spreadsheet
