#ifndef ODR_INTERNAL_ODF_TABLE_H
#define ODR_INTERNAL_ODF_TABLE_H

#include <internal/abstract/table.h>
#include <internal/common/table_position.h>
#include <pugixml.hpp>
#include <unordered_map>

namespace odr::internal::odf {
class OpenDocument;

class Table : public abstract::Table {
public:
  Table(std::shared_ptr<OpenDocument> document, pugi::xml_node node);

  [[nodiscard]] std::shared_ptr<abstract::Document> document() const final;

  [[nodiscard]] TableDimensions
  dimensions(std::uint32_t limit_rows, std::uint32_t limit_cols) const final;

  [[nodiscard]] ElementIdentifier
  cell_first_child(std::uint32_t row, std::uint32_t column) const final;

  std::unordered_map<ElementProperty, std::any>
  column_properties(std::uint32_t column) const final;

  std::unordered_map<ElementProperty, std::any>
  row_properties(std::uint32_t row) const final;

  std::unordered_map<ElementProperty, std::any>
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

  struct Column {
    pugi::xml_node node;
  };

  struct Cell {
    pugi::xml_node node;
    ElementIdentifier first_child;
  };

  struct Row {
    pugi::xml_node node;
  };

  std::shared_ptr<OpenDocument> m_document;
  pugi::xml_node m_node;

  std::unordered_map<std::uint32_t, Column> m_columns;
  std::unordered_map<std::uint32_t, Row> m_rows;
  std::unordered_map<common::TablePosition, Cell> m_cells;

  const Column *column_(std::uint32_t column) const;
  const Row *row_(std::uint32_t row) const;
  const Cell *cell_(std::uint32_t row, std::uint32_t column) const;
};

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_TABLE_H
