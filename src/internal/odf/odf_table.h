#ifndef ODR_INTERNAL_ODF_TABLE_H
#define ODR_INTERNAL_ODF_TABLE_H

#include <internal/abstract/table.h>
#include <internal/common/table_range.h>
#include <map>
#include <pugixml.hpp>
#include <unordered_map>

namespace odr::internal::odf {
class OpenDocument;
class Style;

class Table : public abstract::Table {
public:
  Table(OpenDocument &document, pugi::xml_node node);

  [[nodiscard]] std::shared_ptr<abstract::Document> document() const final;

  [[nodiscard]] TableDimensions dimensions() const final;

  [[nodiscard]] common::TableRange content_bounds() const final;

  [[nodiscard]] ElementIdentifier
  cell_first_child(std::uint32_t row, std::uint32_t column) const final;

  [[nodiscard]] TableDimensions cell_span(std::uint32_t row,
                                          std::uint32_t column) const final;

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  column_properties(std::uint32_t column) const final;

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  row_properties(std::uint32_t row) const final;

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  cell_properties(std::uint32_t row, std::uint32_t column) const final;

  void update_column_properties(
      std::uint32_t column,
      std::unordered_map<ElementProperty, std::any> properties) const final;

  void update_row_properties(
      std::uint32_t row,
      std::unordered_map<ElementProperty, std::any> properties) const final;

  void update_cell_properties(
      std::uint32_t row, std::uint32_t column,
      std::unordered_map<ElementProperty, std::any> properties) const final;

  void resize(std::uint32_t rows, std::uint32_t columns) const final;

  void decouple_cell(std::uint32_t row, std::uint32_t column) const;

private:
  friend class OpenDocument;
  friend class Style;

  class PropertyRegistry;

  struct Column {
    pugi::xml_node node;
  };

  struct Cell {
    pugi::xml_node node;
    ElementIdentifier first_child;
    std::uint32_t rowspan{1};
    std::uint32_t colspan{1};
  };

  struct Row {
    pugi::xml_node node;
    std::map<std::uint32_t, Cell> cells;
  };

  OpenDocument &m_document;

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

#endif // ODR_INTERNAL_ODF_TABLE_H
