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

  [[nodiscard]] bool is_editable() const noexcept override;
  [[nodiscard]] bool is_savable(bool encrypted) const noexcept override;

  void save(const Path &path) const override;
  void save(const Path &path, const char *password) const override;

private:
  XmlDocumentsAndRelations m_xml_documents_and_relations;
  SharedStrings m_shared_strings;

  ElementRegistry m_element_registry;
  StyleRegistry m_style_registry;

  std::pair<pugi::xml_document &, Relations &> parse_xml_(const AbsPath &path);
};

} // namespace odr::internal::ooxml::spreadsheet
