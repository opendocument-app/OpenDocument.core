#include <odr/internal/formula/formula_ast.hpp>

#include <limits>
#include <utility>
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

/// The first and the last row a reference spans after a structural edit,
/// nothing where it is lost.
using RowMove =
    std::function<std::optional<std::pair<std::uint32_t, std::uint32_t>>(
        std::uint32_t first, std::uint32_t last)>;

/// Moves the rows of two corners as one span, so a delete cutting a range
/// shrinks it. A cell is a range of one corner. Whether anything moved,
/// nothing where the reference is lost.
std::optional<bool> move_range(CellReference &from, CellReference &to,
                               const EditedSheet &edited, const RowMove &move) {
  // the second corner's unstated sheet is the first's
  if (from.document.has_value() || to.document.has_value() ||
      !from.row.has_value() || !to.row.has_value() || !edited(from.sheet) ||
      !edited(to.sheet.has_value() ? to.sheet : from.sheet)) {
    return false;
  }
  const bool ascending = from.row->index <= to.row->index;
  Coordinate &first = ascending ? *from.row : *to.row;
  Coordinate &last = ascending ? *to.row : *from.row;
  const auto rows = move(first.index, last.index);
  if (!rows.has_value()) {
    return std::nullopt;
  }
  const bool moved = rows->first != first.index || rows->second != last.index;
  first.index = rows->first;
  last.index = rows->second;
  return moved;
}

bool move_rows(Node &node, const EditedSheet &edited, const RowMove &move) {
  CellReference *from = nullptr;
  CellReference *to = nullptr;
  if (auto *cell = std::get_if<CellReference>(&node.content)) {
    from = to = cell;
  } else if (auto *range = std::get_if<RangeReference>(&node.content)) {
    from = &range->from;
    to = &range->to;
  } else if (const auto *binary = std::get_if<BinaryOperation>(&node.content);
             binary != nullptr && binary->op == BinaryOperator::range &&
             node.children.size() == 2) {
    // a range the file spelled as two nodes around `:` rather than as one
    // token
    from = std::get_if<CellReference>(&node.children.front().content);
    to = std::get_if<CellReference>(&node.children.back().content);
    if (from == nullptr || to == nullptr) {
      from = to = nullptr;
    }
  }

  if (from != nullptr) {
    const std::optional<bool> moved = move_range(*from, *to, edited, move);
    if (!moved.has_value()) {
      node.content = ErrorLiteral{ErrorType::reference};
      node.children.clear();
      return true;
    }
    return *moved;
  }
  bool moved = false;
  for (Node &child : node.children) {
    moved = move_rows(child, edited, move) || moved;
  }
  return moved;
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

bool formula::insert_rows(Node &node, const EditedSheet &edited,
                          const std::uint32_t row, const std::uint32_t count) {
  return move_rows(
      node, edited,
      [&](const std::uint32_t first, const std::uint32_t last)
          -> std::optional<std::pair<std::uint32_t, std::uint32_t>> {
        if (last >= row &&
            static_cast<std::int64_t>(last) + count > index_limit) {
          return std::nullopt;
        }
        const auto at = [&](const std::uint32_t index) {
          return index < row ? index : index + count;
        };
        return std::pair(at(first), at(last));
      });
}

bool formula::delete_rows(Node &node, const EditedSheet &edited,
                          const std::uint32_t row, const std::uint32_t count) {
  const std::uint64_t end = static_cast<std::uint64_t>(row) + count;
  return move_rows(
      node, edited,
      [&](const std::uint32_t first, const std::uint32_t last)
          -> std::optional<std::pair<std::uint32_t, std::uint32_t>> {
        if (first >= row && last < end) {
          return std::nullopt;
        }
        const std::uint32_t moved_first = first < row   ? first
                                          : first < end ? row
                                                        : first - count;
        const std::uint32_t moved_last = last < row   ? last
                                         : last < end ? row - 1
                                                      : last - count;
        return std::pair(moved_first, moved_last);
      });
}

} // namespace odr::internal
