#include <odr/internal/formula/formula_dependencies.hpp>

#include <algorithm>
#include <limits>
#include <variant>

namespace odr::internal::formula {

namespace {

constexpr std::uint32_t index_limit = std::numeric_limits<std::uint32_t>::max();

/// The rectangle two corners span. An axis neither corner states reaches the
/// whole sheet; the second corner's unstated sheet is the first's.
Extent extent_of(const CellReference &from, const CellReference &to) {
  const auto index = [](const std::optional<Coordinate> &axis,
                        const std::uint32_t unstated) {
    return axis.has_value() ? axis->index : unstated;
  };
  const std::uint32_t from_column = index(from.column, 0);
  const std::uint32_t to_column = index(to.column, index_limit);
  const std::uint32_t from_row = index(from.row, 0);
  const std::uint32_t to_row = index(to.row, index_limit);
  return Extent{
      from.document, from.sheet,
      TableRange{
          {std::min(from_column, to_column), std::min(from_row, to_row)},
          {std::max(from_column, to_column), std::max(from_row, to_row)}}};
}

const CellReference *as_cell(const Node &node) {
  return std::get_if<CellReference>(&node.content);
}

void collect(const Node &node, References &into) {
  if (const auto *cell = as_cell(node)) {
    into.extents.push_back(extent_of(*cell, *cell));
    return;
  }
  if (const auto *range = std::get_if<RangeReference>(&node.content)) {
    into.extents.push_back(extent_of(range->from, range->to));
    return;
  }
  if (node.holds<NameReference>()) {
    into.complete = false;
    return;
  }
  // a range the file spelled as two nodes around `:` rather than as one token
  if (const auto *binary = std::get_if<BinaryOperation>(&node.content);
      binary != nullptr && binary->op == BinaryOperator::range &&
      node.children.size() == 2) {
    const CellReference *from = as_cell(node.children.front());
    const CellReference *to = as_cell(node.children.back());
    if (from != nullptr && to != nullptr) {
      into.extents.push_back(extent_of(*from, *to));
      return;
    }
    // a corner that is no reference: what the range spans cannot be read
    into.complete = false;
  }
  for (const Node &child : node.children) {
    collect(child, into);
  }
}

} // namespace

} // namespace odr::internal::formula

namespace odr::internal {

formula::References formula::references(const Node &node) {
  References result;
  collect(node, result);
  return result;
}

} // namespace odr::internal
