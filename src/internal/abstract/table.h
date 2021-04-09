#ifndef ODR_INTERNAL_ABSTRACT_TABLE_H
#define ODR_INTERNAL_ABSTRACT_TABLE_H

#include <any>
#include <internal/identifier.h>

namespace odr {
enum class ElementProperty;
struct TableDimensions;
} // namespace odr

namespace odr::internal::abstract {
class Document;

class Table {
public:
  virtual ~Table() = default;

  [[nodiscard]] virtual std::shared_ptr<abstract::Document>
  document() const = 0;

  /// \param element_id the element to query.
  /// \param row the requested row.
  /// \param column the requested column.
  /// \return the first child of the table cell.
  [[nodiscard]] virtual ElementIdentifier
  cell_first_child(std::uint32_t row, std::uint32_t column) const = 0;

  /// \param element_id the element to query.
  /// \param column the requested column.
  /// \param property the requested property.
  /// \return the requested optional value.
  [[nodiscard]] virtual std::any
  column_property(std::uint32_t column, ElementProperty property) const = 0;

  /// \param element_id the element to query.
  /// \param row the requested row.
  /// \param property the requested property.
  /// \return the requested optional value.
  [[nodiscard]] virtual std::any
  row_property(std::uint32_t row, ElementProperty property) const = 0;

  /// \param element_id the element to query.
  /// \param row the requested row.
  /// \param column the requested column.
  /// \param property the requested property.
  /// \return the requested optional value.
  [[nodiscard]] virtual std::any
  cell_property(std::uint32_t row, std::uint32_t column,
                ElementProperty property) const = 0;

  /// \param element_id the element to query.
  /// \param limit_rows
  /// \param limit_columns
  /// \return the requested table dimensions.
  [[nodiscard]] virtual TableDimensions
  dimensions(std::uint32_t limit_rows, std::uint32_t limit_columns) const = 0;

  virtual void resize(std::uint32_t rows, std::uint32_t columns) const = 0;
};

} // namespace odr::internal::abstract

#endif // ODR_INTERNAL_ABSTRACT_TABLE_H
