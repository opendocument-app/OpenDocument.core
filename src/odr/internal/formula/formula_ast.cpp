#include <odr/internal/formula/formula_ast.hpp>

#include <odr/internal/util/string_util.hpp>

#include <algorithm>
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
  const auto index = static_cast<std::int64_t>(coordinate->index);
  if (by < -index || by > index_limit - index) {
    return false;
  }
  coordinate->index = static_cast<std::uint32_t>(index + by);
  return true;
}

[[nodiscard]] bool shift_cell(CellReference &cell, const std::int64_t columns,
                              const std::int64_t rows) {
  return shift_coordinate(cell.column, columns) &&
         shift_coordinate(cell.row, rows);
}

/// OOXML keeps a sheet span as its original spelling, including quotes.
std::optional<std::pair<std::string, std::string>>
sheet_span(const std::optional<std::string> &sheet) {
  if (!sheet.has_value() || sheet->find(':') == std::string::npos) {
    return std::nullopt;
  }
  std::string names;
  bool quoted = false;
  for (std::size_t i = 0; i < sheet->size(); ++i) {
    const char c = (*sheet)[i];
    if (c != '\'') {
      names += c;
    } else if (quoted && i + 1 < sheet->size() && (*sheet)[i + 1] == '\'') {
      names += c;
      ++i;
    } else {
      quoted = !quoted;
    }
  }
  const auto colon = names.find(':');
  return std::pair(names.substr(0, colon), names.substr(colon + 1));
}

std::string quote_sheet_span(const std::string &first,
                             const std::string &last) {
  std::string result = "'";
  const auto append = [&](const std::string &name) {
    for (const char c : name) {
      result += c;
      if (c == '\'') {
        result += c;
      }
    }
  };
  append(first);
  result += ':';
  append(last);
  return result + "'";
}

/// Moves the coordinates of two corners on the edited axis as one span, so a
/// delete cutting a range shrinks it. A cell is a range of one corner.
/// Whether anything moved, nothing where the reference is lost.
std::optional<bool> move_range(CellReference &from, CellReference &to,
                               const SheetEdit &edit,
                               const std::optional<std::string> &sheet) {
  if (!edit.insert && !edit.sheet_order.empty() && !from.document.has_value() &&
      !to.document.has_value()) {
    if (const auto span = sheet_span(from.sheet); span.has_value()) {
      CellReference first{.sheet = span->first};
      CellReference last{.sheet = span->second};
      const auto moved = move_range(first, last, edit, sheet);
      if (!moved.has_value() ||
          (util::string::equals_ignore_case(span->first, edit.sheet) &&
           util::string::equals_ignore_case(span->second, edit.sheet))) {
        return std::nullopt;
      }
      if (*moved) {
        const bool same_sheet = to.sheet == from.sheet;
        from.sheet = util::string::equals_ignore_case(*first.sheet, *last.sheet)
                         ? *first.sheet
                         : quote_sheet_span(*first.sheet, *last.sheet);
        if (&from != &to && same_sheet) {
          to.sheet = from.sheet;
        }
      }
      return moved;
    }
  }
  std::optional<Coordinate> CellReference::*const axis =
      edit.axis == Axis::row ? &CellReference::row : &CellReference::column;
  auto &from_axis = from.*axis;
  auto &to_axis = to.*axis;
  // the second corner's unstated sheet is the first's
  const std::optional<std::string> &from_sheet =
      from.sheet.has_value() ? from.sheet : sheet;
  const std::optional<std::string> &to_sheet =
      to.sheet.has_value() ? to.sheet : from_sheet;
  if (!edit.insert && !edit.sheet_order.empty() && !from.document.has_value() &&
      !to.document.has_value() && from_sheet.has_value() &&
      to_sheet.has_value() &&
      !util::string::equals_ignore_case(*from_sheet, *to_sheet)) {
    const bool remove_from =
        util::string::equals_ignore_case(*from_sheet, edit.sheet);
    const bool remove_to =
        util::string::equals_ignore_case(*to_sheet, edit.sheet);
    if (!remove_from && !remove_to) {
      return false;
    }
    const auto find_sheet = [&](const std::string &name) {
      return std::ranges::find_if(edit.sheet_order, [&](const std::string &s) {
        return util::string::equals_ignore_case(s, name);
      });
    };
    const auto first = find_sheet(*from_sheet);
    const auto last = find_sheet(*to_sheet);
    if (first == edit.sheet_order.end() || last == edit.sheet_order.end()) {
      return std::nullopt;
    }
    if (remove_from) {
      from.sheet = *(first + (first < last ? 1 : -1));
    } else {
      to.sheet = *(last + (first < last ? -1 : 1));
    }
    return true;
  }
  if (from.document.has_value() || to.document.has_value() ||
      !from_axis.has_value() || !to_axis.has_value() ||
      !from_sheet.has_value() || !to_sheet.has_value() ||
      !util::string::equals_ignore_case(*from_sheet, edit.sheet) ||
      !util::string::equals_ignore_case(*to_sheet, edit.sheet)) {
    return false;
  }
  const bool ascending = from_axis->index <= to_axis->index;
  Coordinate &first = ascending ? *from_axis : *to_axis;
  Coordinate &last = ascending ? *to_axis : *from_axis;
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
