#pragma once

#include <odr/internal/common/document.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/ooxml/ooxml_util.hpp>
#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_element_registry.hpp>
#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_style.hpp>

#include <memory>
#include <unordered_map>
#include <vector>

#include <pugixml.hpp>

namespace odr::internal::ooxml::spreadsheet {

class Document final : public internal::Document {
public:
  explicit Document(std::shared_ptr<abstract::ReadableFilesystem> files);

  [[nodiscard]] const ElementRegistry &element_registry() const;
  [[nodiscard]] const StyleRegistry &style_registry() const;

private:
  XmlDocumentsAndRelations m_xml_documents_and_relations;
  SharedStrings m_shared_strings;

  ElementRegistry m_element_registry;
  StyleRegistry m_style_registry;

  std::pair<pugi::xml_document &, Relations &> parse_xml_(const AbsPath &path);
};

} // namespace odr::internal::ooxml::spreadsheet
