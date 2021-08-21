#include <internal/abstract/table.h>
#include <internal/common/table_range.h>
#include <odr/element.h>
#include <odr/table.h>

namespace odr {

TableDimensions::TableDimensions() = default;

TableDimensions::TableDimensions(const std::uint32_t rows,
                                 const std::uint32_t columns)
    : rows{rows}, columns{columns} {}

Table::Table() = default;

TableDimensions Table::dimensions() const {
  if (!operator bool()) {
    return {};
  }
  return m_thing->dimensions();
}

TableDimensions Table::content_bounds() const {
  if (!operator bool()) {
    return {};
  }
  auto result = m_thing->content_bounds();
  return {result.to().row(), result.to().column()};
}

TableDimensions Table::content_bounds(TableDimensions within) const {
  if (!operator bool()) {
    return {};
  }
  auto result =
      m_thing->content_bounds({{0, 0}, {within.rows, within.columns}});
  return {result.to().row(), result.to().column()};
}

Element Table::column(const std::uint32_t column) const {
  if (!operator bool()) {
    return {};
  }
  return m_thing->column(column);
}

Element Table::row(const std::uint32_t row) const {
  if (!operator bool()) {
    return {};
  }
  return m_thing->row(row);
}

Element Table::cell(const std::uint32_t row, const std::uint32_t column) const {
  if (!operator bool()) {
    return {};
  }
  return m_thing->cell(row, column);
}

} // namespace odr
