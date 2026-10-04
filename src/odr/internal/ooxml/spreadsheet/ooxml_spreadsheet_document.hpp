#pragma once

#include <odr/internal/common/document.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/ooxml/ooxml_util.hpp>
#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_element_registry.hpp>
#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_style.hpp>

#include <iosfwd>
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
  StyleRegistry &style_registry();

  /// Where a date serial counts from, as `workbookPr/@date1904` says.
  [[nodiscard]] number_format::Epoch epoch() const;

  /// The `workbook` element of `xl/workbook.xml`.
  [[nodiscard]] pugi::xml_node workbook() const;
  /// The `calcChain` element (ECMA-376 18.6), read on first use and written
  /// back by `save`. Null where the package has none.
  [[nodiscard]] pugi::xml_node calc_chain();

  [[nodiscard]] bool is_editable() const noexcept override;
  [[nodiscard]] bool is_savable(bool encrypted) const noexcept override;

  void save(std::ostream &out) const override;
  void save(std::ostream &out, const char *password) const override;

private:
  XmlDocumentsAndRelations m_xml_documents_and_relations;
  SharedStrings m_shared_strings;
  /// The parts `save` writes back from their dom; the rest is byte-copied.
  std::vector<AbsPath> m_written_parts;

  ElementRegistry m_element_registry;
  StyleRegistry m_style_registry;
  number_format::Epoch m_epoch{number_format::Epoch::from_1900};

  std::pair<pugi::xml_document &, Relations &> parse_xml_(const AbsPath &path);
};

} // namespace odr::internal::ooxml::spreadsheet
