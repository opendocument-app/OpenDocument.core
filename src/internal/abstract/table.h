#ifndef ODR_INTERNAL_ABSTRACT_TABLE_H
#define ODR_INTERNAL_ABSTRACT_TABLE_H

#include <any>
#include <internal/identifier.h>
#include <memory>
#include <unordered_map>

namespace odr {
enum class ElementProperty;
struct TableDimensions;
} // namespace odr

namespace odr::internal::common {
class TableRange;
} // namespace odr::internal::common

namespace odr::internal::abstract {
class Document;

class Table {
public:
  virtual ~Table() = default;

  [[nodiscard]] virtual std::shared_ptr<abstract::Document>
  document() const = 0;

  /// \param element_id the element to query.
  /// \return the requested table dimensions.
  [[nodiscard]] virtual TableDimensions dimensions() const = 0;

  [[nodiscard]] virtual common::TableRange content_bounds() const = 0;

  /// \param element_id the element to query.
  /// \param row the requested row.
  /// \param column the requested column.
  /// \return the first child of the table cell.
  [[nodiscard]] virtual ElementIdentifier
  cell_first_child(std::uint32_t row, std::uint32_t column) const = 0;

  [[nodiscard]] virtual TableDimensions
  cell_span(std::uint32_t row, std::uint32_t column) const = 0;

  [[nodiscard]] virtual std::unordered_map<ElementProperty, std::any>
  column_properties(std::uint32_t column) const = 0;

  [[nodiscard]] virtual std::unordered_map<ElementProperty, std::any>
  row_properties(std::uint32_t row) const = 0;

  [[nodiscard]] virtual std::unordered_map<ElementProperty, std::any>
  cell_properties(std::uint32_t row, std::uint32_t column) const = 0;

  virtual void update_column_properties(
      std::uint32_t column,
      std::unordered_map<ElementProperty, std::any> properties) const = 0;

  virtual void update_row_properties(
      std::uint32_t row,
      std::unordered_map<ElementProperty, std::any> properties) const = 0;

  virtual void update_cell_properties(
      std::uint32_t row, std::uint32_t column,
      std::unordered_map<ElementProperty, std::any> properties) const = 0;

  virtual void resize(std::uint32_t rows, std::uint32_t columns) const = 0;
};

} // namespace odr::internal::abstract

#endif // ODR_INTERNAL_ABSTRACT_TABLE_H
