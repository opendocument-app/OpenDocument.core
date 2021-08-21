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
  return get()->dimensions();
}

TableDimensions Table::content_bounds() const {
  if (!operator bool()) {
    return {};
  }
  auto result = get()->content_bounds();
  return {result.to().row(), result.to().column()};
}

TableDimensions Table::content_bounds(TableDimensions within) const {
  if (!operator bool()) {
    return {};
  }
  auto result = get()->content_bounds({{0, 0}, {within.rows, within.columns}});
  return {result.to().row(), result.to().column()};
}

Element Table::column(const std::uint32_t column) const {
  if (!operator bool()) {
    return {};
  }
  return get()->column(column);
}

Element Table::row(const std::uint32_t row) const {
  if (!operator bool()) {
    return {};
  }
  return get()->row(row);
}

Element Table::cell(const std::uint32_t row, const std::uint32_t column) const {
  if (!operator bool()) {
    return {};
  }
  return get()->cell(row, column);
}

} // namespace odr
