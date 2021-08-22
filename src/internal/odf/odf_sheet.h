#ifndef ODR_INTERNAL_ODF_SHEET_H
#define ODR_INTERNAL_ODF_SHEET_H

#include <internal/common/table_range.h>
#include <internal/odf/odf_element.h>
#include <map>
#include <pugixml.hpp>
#include <unordered_map>

namespace odr::internal::odf {
class OpenDocument;
class Style;

class Sheet : public abstract::Sheet {
public:
  Sheet(pugi::xml_node node, OpenDocumentSpreadsheet *document);

  [[nodiscard]] TableDimensions dimensions() const final;

  [[nodiscard]] common::TableRange content_bounds() const final;
  [[nodiscard]] common::TableRange
  content_bounds(common::TableRange within) const final;

  [[nodiscard]] odr::Element column(std::uint32_t column) const final;

  [[nodiscard]] odr::Element row(std::uint32_t row) const final;

  [[nodiscard]] odr::Element cell(std::uint32_t row,
                                  std::uint32_t column) const final;

private:
  struct Column {
    pugi::xml_node node;
  };

  struct Cell {
    pugi::xml_node node;
    std::uint32_t rowspan{1};
    std::uint32_t colspan{1};
  };

  struct Row {
    pugi::xml_node node;
    std::map<std::uint32_t, Cell> cells;
  };

  OpenDocumentSpreadsheet *m_document;

  TableDimensions m_dimensions;
  common::TableRange m_content_bounds;

  std::map<std::uint32_t, Column> m_columns;
  std::map<std::uint32_t, Row> m_rows;

  void register_(pugi::xml_node node);

  [[nodiscard]] const Column *column_(std::uint32_t column) const;
  [[nodiscard]] const Row *row_(std::uint32_t row) const;
  [[nodiscard]] const Cell *cell_(std::uint32_t row,
                                  std::uint32_t column) const;
};

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_SHEET_H
