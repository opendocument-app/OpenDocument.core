#ifndef ODR_INTERNAL_ODF_TABLE_H
#define ODR_INTERNAL_ODF_TABLE_H

#include <internal/common/table_range.h>
#include <internal/odf/odf_element.h>
#include <map>
#include <pugixml.hpp>
#include <unordered_map>

namespace odr::internal::odf {
class OpenDocument;
class Style;

class Table : public abstract::Table {
public:
  Table(OpenDocument &document, odf::TableElement element);

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
    odf::TableColumnElement element;
  };

  struct Cell {
    odf::TableCellElement element;
    std::uint32_t rowspan{1};
    std::uint32_t colspan{1};
  };

  struct Row {
    odf::TableRowElement element;
    std::map<std::uint32_t, Cell> cells;
  };

  OpenDocument &m_document;

  TableDimensions m_dimensions;
  common::TableRange m_content_bounds;

  std::map<std::uint32_t, Column> m_columns;
  std::map<std::uint32_t, Row> m_rows;

  void register_(odf::TableElement element);

  [[nodiscard]] const Column *column_(std::uint32_t column) const;
  [[nodiscard]] const Row *row_(std::uint32_t row) const;
  [[nodiscard]] const Cell *cell_(std::uint32_t row,
                                  std::uint32_t column) const;
};

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_TABLE_H
