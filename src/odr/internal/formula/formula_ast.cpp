#include <odr/internal/formula/formula_ast.hpp>

#include <limits>
#include <variant>

namespace odr::internal::formula {

namespace {

constexpr std::int64_t index_limit = std::numeric_limits<std::uint32_t>::max();

[[nodiscard]] bool shift_coordinate(std::optional<Coordinate> &coordinate,
                                    const std::int64_t by) {
  if (!coordinate.has_value() || coordinate->absolute) {
    return true;
  }
  const std::int64_t moved = static_cast<std::int64_t>(coordinate->index) + by;
  if (moved < 0 || moved > index_limit) {
    return false;
  }
  coordinate->index = static_cast<std::uint32_t>(moved);
  return true;
}

[[nodiscard]] bool shift_cell(CellReference &cell, const std::int64_t columns,
                              const std::int64_t rows) {
  return shift_coordinate(cell.column, columns) &&
         shift_coordinate(cell.row, rows);
}

} // namespace

} // namespace odr::internal::formula

namespace odr::internal {

void formula::shift(Node &node, const std::int64_t columns,
                    const std::int64_t rows) {
  if (auto *cell = std::get_if<CellReference>(&node.content)) {
    if (!shift_cell(*cell, columns, rows)) {
      node.content = ErrorLiteral{ErrorType::reference};
    }
  } else if (auto *range = std::get_if<RangeReference>(&node.content)) {
    if (!shift_cell(range->from, columns, rows) ||
        !shift_cell(range->to, columns, rows)) {
      node.content = ErrorLiteral{ErrorType::reference};
    }
  }
  for (Node &child : node.children) {
    shift(child, columns, rows);
  }
}

} // namespace odr::internal
