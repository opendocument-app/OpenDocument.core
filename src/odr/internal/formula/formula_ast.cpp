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

/// Moves the coordinates of two corners on the edited axis as one span, so a
/// delete cutting a range shrinks it. A cell is a range of one corner.
/// Whether anything moved, nothing where the reference is lost.
std::optional<bool> move_range(CellReference &from, CellReference &to,
                               const SheetEdit &edit,
                               const std::optional<std::string> &sheet) {
  std::optional<Coordinate> CellReference::*const axis =
      edit.axis == Axis::row ? &CellReference::row : &CellReference::column;
  // the second corner's unstated sheet is the first's
  const std::optional<std::string> &from_sheet =
      from.sheet.has_value() ? from.sheet : sheet;
  const std::optional<std::string> &to_sheet =
      to.sheet.has_value() ? to.sheet : from_sheet;
  if (from.document.has_value() || to.document.has_value() ||
      !(from.*axis).has_value() || !(to.*axis).has_value() ||
      from_sheet != edit.sheet || to_sheet != edit.sheet) {
    return false;
  }
  const bool ascending = (from.*axis)->index <= (to.*axis)->index;
  Coordinate &first = ascending ? *(from.*axis) : *(to.*axis);
  Coordinate &last = ascending ? *(to.*axis) : *(from.*axis);
  const auto span = edit.span(first.index, last.index);
  if (!span.has_value()) {
    return std::nullopt;
  }
  const bool moved = span->first != first.index || span->second != last.index;
  first.index = span->first;
  last.index = span->second;
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

std::optional<std::pair<std::uint32_t, std::uint32_t>>
formula::SheetEdit::span(const std::uint32_t first,
                         const std::uint32_t last) const {
  const std::uint64_t end = static_cast<std::uint64_t>(index) + count;
  if (insert) {
    if (last >= index &&
        static_cast<std::int64_t>(last) + count > index_limit) {
      return std::nullopt;
    }
    const auto at = [&](const std::uint32_t coordinate) {
      return coordinate < index ? coordinate : coordinate + count;
    };
    return std::pair(at(first), at(last));
  }
  if (first >= index && last < end) {
    return std::nullopt;
  }
  return std::pair(first < index ? first
                   : first < end ? index
                                 : first - count,
                   last < index ? last
                   : last < end ? index - 1
                                : last - count);
}

bool formula::move_references(Node &node, const SheetEdit &edit,
                              const std::optional<std::string> &sheet) {
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
    const std::optional<bool> moved = move_range(*from, *to, edit, sheet);
    if (!moved.has_value()) {
      node.content = ErrorLiteral{ErrorType::reference};
      node.children.clear();
      return true;
    }
    return *moved;
  }
  bool moved = false;
  for (Node &child : node.children) {
    moved = move_references(child, edit, sheet) || moved;
  }
  return moved;
}

} // namespace odr::internal
