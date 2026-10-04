#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_references.hpp>

#include <odr/table_position.hpp>

#include <odr/internal/formula/formula_parser.hpp>
#include <odr/internal/formula/formula_writer.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <optional>
#include <ranges>
#include <string_view>
#include <utility>

#include <fmt/format.h>
#include <fmt/ranges.h>

namespace odr::internal::ooxml::spreadsheet {

namespace {

constexpr formula::Syntax syntax = formula::Syntax::ooxml;

/// Where the cell at @p position sits after the edit, nothing where it is
/// removed.
std::optional<TablePosition> moved_position(const TablePosition &position,
                                            const bool edited,
                                            const formula::RowEdit &edit) {
  if (!edited) {
    return position;
  }
  const auto rows = edit.span(position.row, position.row);
  if (!rows.has_value()) {
    return std::nullopt;
  }
  return TablePosition(position.column, rows->first);
}

/// @p node as the cell at @p to reads it, where the one at @p from states it.
formula::Node shifted(formula::Node node, const TablePosition &from,
                      const TablePosition &to) {
  formula::shift(node, static_cast<std::int64_t>(to.column) - from.column,
                 static_cast<std::int64_t>(to.row) - from.row);
  return node;
}

void move_ref(pugi::xml_node formula, const std::string &sheet,
              const formula::RowEdit &edit) {
  if (pugi::xml_attribute ref = formula.attribute("ref")) {
    if (const std::optional<std::string> moved =
            formula::move_row_addresses(ref.value(), edit, sheet, syntax)) {
      ref.set_value(moved->c_str());
    }
  }
}

/// Moves the expression @p node states, where it parses.
void move_text(pugi::xml_node node, const std::optional<std::string> &sheet,
               const formula::RowEdit &edit) {
  if (std::optional<formula::Node> expression =
          formula::parse(node.text().get(), syntax);
      expression.has_value() && formula::move_rows(*expression, edit, sheet)) {
    node.text().set(formula::to_string(*expression, syntax).c_str());
  }
}

struct Member final {
  pugi::xml_node formula;
  TablePosition position;
};

/// Keeps a shared group where its master, moved, still says what each member
/// reads after the edit. Else every member that stays states its own formula.
void move_shared_group(const std::vector<Member> &members,
                       const std::string &sheet, const bool edited,
                       const formula::RowEdit &edit) {
  const auto master = std::ranges::find_if(members, [](const Member &member) {
    return !std::string_view(member.formula.text().get()).empty();
  });
  if (master == members.end()) {
    return;
  }
  const std::optional<formula::Node> expression =
      formula::parse(master->formula.text().get(), syntax);
  if (!expression.has_value()) {
    return;
  }
  formula::Node moved_master = *expression;
  const bool master_moved = formula::move_rows(moved_master, edit, sheet);
  const std::optional<TablePosition> master_at =
      moved_position(master->position, edited, edit);

  bool kept = master_at.has_value();
  std::vector<std::pair<pugi::xml_node, std::string>> written;
  for (const Member &member : members) {
    const std::optional<TablePosition> member_at =
        moved_position(member.position, edited, edit);
    if (!member_at.has_value()) {
      continue;
    }
    formula::Node expected =
        shifted(*expression, master->position, member.position);
    formula::move_rows(expected, edit, sheet);
    std::string text = formula::to_string(expected, syntax);
    kept = kept &&
           formula::to_string(shifted(moved_master, *master_at, *member_at),
                              syntax) == text;
    written.emplace_back(member.formula, std::move(text));
  }

  if (kept) {
    if (master_moved) {
      master->formula.text().set(
          formula::to_string(moved_master, syntax).c_str());
    }
    move_ref(master->formula, sheet, edit);
    return;
  }
  for (auto &[formula, text] : written) {
    formula.remove_attribute("t");
    formula.remove_attribute("si");
    formula.remove_attribute("ref");
    formula.text().set(text.c_str());
  }
}

void move_worksheet(const NamedWorksheet &worksheet, const bool edited,
                    const formula::RowEdit &edit) {
  std::map<std::string, std::vector<Member>> groups;
  for (const pugi::xml_node row :
       worksheet.node.child("sheetData").children("row")) {
    for (const pugi::xml_node cell : row.children("c")) {
      const pugi::xml_node formula = cell.child("f");
      if (!formula) {
        continue;
      }
      if (std::string_view(formula.attribute("t").value()) == "shared") {
        groups[formula.attribute("si").value()].push_back(
            {formula, TablePosition(cell.attribute("r").value())});
        continue;
      }
      move_text(formula, worksheet.name, edit);
      move_ref(formula, worksheet.name, edit);
    }
  }
  for (const auto &[unused, members] : groups) {
    move_shared_group(members, worksheet.name, edited, edit);
  }
}

/// ECMA-376 18.6.1: an entry without `i` is on the sheet of the one before.
void move_calc_chain(pugi::xml_node calc_chain,
                     const std::string &edited_sheet_id,
                     const formula::RowEdit &edit) {
  std::string sheet_id;
  for (pugi::xml_node entry = calc_chain.first_child(); entry;) {
    pugi::xml_node next = entry.next_sibling();
    if (const pugi::xml_attribute id = entry.attribute("i")) {
      sheet_id = id.value();
    }
    if (sheet_id == edited_sheet_id) {
      if (const std::optional<TablePosition> at = moved_position(
              TablePosition(entry.attribute("r").value()), true, edit)) {
        entry.attribute("r").set_value(at->to_string().c_str());
      } else {
        if (next && !next.attribute("i") && entry.attribute("i")) {
          next.prepend_attribute("i").set_value(sheet_id.c_str());
        }
        calc_chain.remove_child(entry);
      }
    }
    entry = next;
  }
}

/// An element stating a range list, which goes when a delete takes the list.
struct RangeAttribute final {
  std::string_view element;
  std::string_view attribute;
};

constexpr std::array<RangeAttribute, 7> removable_ranges{{
    {"conditionalFormatting", "sqref"},
    {"dataValidation", "sqref"},
    {"hyperlink", "ref"},
    {"autoFilter", "ref"},
    {"sortState", "ref"},
    {"protectedRange", "sqref"},
    {"ignoredError", "sqref"},
}};

/// The cell @p address names after the edit, the first one past the removed
/// rows where a delete takes it.
std::string moved_cell(const std::string &address,
                       const formula::RowEdit &edit) {
  const TablePosition position(address);
  const auto rows = edit.span(position.row, position.row);
  return TablePosition(position.column,
                       rows.has_value() ? rows->first : edit.row)
      .to_string();
}

void collect_ranges(const pugi::xml_node node, const formula::RowEdit &edit,
                    std::vector<pugi::xml_node> &lost) {
  for (pugi::xml_node child : node.children()) {
    const std::string_view name = child.name();
    if (child.type() != pugi::node_element || name == "sheetData") {
      continue;
    }
    for (const RangeAttribute &range : removable_ranges) {
      pugi::xml_attribute attribute = child.attribute(range.attribute.data());
      if (name != range.element || !attribute) {
        continue;
      }
      if (const std::optional<std::string> moved = formula::move_row_addresses(
              attribute.value(), edit, edit.sheet, syntax)) {
        if (moved->empty()) {
          lost.push_back(child);
        } else {
          attribute.set_value(moved->c_str());
        }
      }
    }
    if (name == "selection") {
      pugi::xml_attribute active = child.attribute("activeCell");
      if (active) {
        active.set_value(moved_cell(active.value(), edit).c_str());
      }
      if (pugi::xml_attribute sqref = child.attribute("sqref")) {
        if (const std::optional<std::string> moved =
                formula::move_row_addresses(sqref.value(), edit, edit.sheet,
                                            syntax)) {
          if (!moved->empty()) {
            sqref.set_value(moved->c_str());
          } else if (active) {
            sqref.set_value(active.value());
          } else {
            child.remove_attribute(sqref); // `A1`, the default
          }
        }
      }
    }
    if (pugi::xml_attribute top_left = child.attribute("topLeftCell");
        top_left && (name == "pane" || name == "sheetView")) {
      top_left.set_value(moved_cell(top_left.value(), edit).c_str());
    }
    collect_ranges(child, edit, lost);
  }
}

/// The row of an anchor corner after the edit, at the edge of the rows that
/// stay where a delete takes it.
void move_corner(pugi::xml_node corner, const formula::RowEdit &edit) {
  pugi::xml_text row = corner.child("xdr:row").text();
  if (const auto rows = edit.span(row.as_uint(), row.as_uint())) {
    row.set(rows->first);
  } else {
    row.set(edit.row);
    corner.child("xdr:rowOff").text().set(0);
  }
}

} // namespace

} // namespace odr::internal::ooxml::spreadsheet

namespace odr::internal {

void ooxml::spreadsheet::move_row_references(
    const pugi::xml_node workbook,
    const std::vector<NamedWorksheet> &worksheets,
    const pugi::xml_node calc_chain, const std::string &edited_sheet_id,
    const formula::RowEdit &edit) {
  for (const NamedWorksheet &worksheet : worksheets) {
    move_worksheet(worksheet, worksheet.name == edit.sheet, edit);
  }

  // ECMA-376 18.2.5: a name local to a sheet states its index among them
  for (const pugi::xml_node name :
       workbook.child("definedNames").children("definedName")) {
    std::optional<std::string> sheet;
    if (const pugi::xml_attribute local = name.attribute("localSheetId");
        local && local.as_uint() < worksheets.size()) {
      sheet = worksheets[local.as_uint()].name;
    }
    move_text(name, sheet, edit);
  }

  if (calc_chain) {
    move_calc_chain(calc_chain, edited_sheet_id, edit);
  }
}

void ooxml::spreadsheet::move_sheet_ranges(const pugi::xml_node worksheet,
                                           const formula::RowEdit &edit) {
  std::vector<pugi::xml_node> lost;
  collect_ranges(worksheet, edit, lost);
  for (pugi::xml_node node : lost) {
    pugi::xml_node parent = node.parent();
    parent.remove_child(node);
    // a list such as `hyperlinks` states one entry at least
    if (parent == worksheet) {
      continue;
    }
    if (!parent.find_child([](const pugi::xml_node child) {
          return child.type() == pugi::node_element;
        })) {
      parent.parent().remove_child(parent);
    } else if (pugi::xml_attribute count = parent.attribute("count")) {
      count.set_value(static_cast<std::uint32_t>(
          std::ranges::distance(parent.children(node.name()))));
    }
  }
}

void ooxml::spreadsheet::move_drawing(const pugi::xml_node drawing,
                                      const formula::RowEdit &edit) {
  for (const pugi::xml_node anchor : drawing.children()) {
    const std::string_view edit_as = anchor.attribute("editAs").value();
    const pugi::xml_node from = anchor.child("xdr:from");
    if (!from || edit_as == "absolute") {
      continue;
    }
    const pugi::xml_text from_row = from.child("xdr:row").text();
    const std::int64_t old = from_row.as_uint();
    move_corner(from, edit);
    const pugi::xml_node to = anchor.child("xdr:to");
    if (!to) {
      continue;
    }
    if (edit_as != "oneCell") {
      move_corner(to, edit);
    } else {
      // a `oneCell` box keeps its size, so its far corner moves as the first
      pugi::xml_text to_row = to.child("xdr:row").text();
      to_row.set(static_cast<std::uint32_t>(std::max<std::int64_t>(
          0, to_row.as_llong() + from_row.as_llong() - old)));
    }
  }
}

void ooxml::spreadsheet::move_comments(const pugi::xml_node comments,
                                       const pugi::xml_node threaded,
                                       const pugi::xml_node vml,
                                       const formula::RowEdit &edit) {
  const auto move_refs = [&](pugi::xml_node list, const char *name) {
    for (pugi::xml_node comment = list.child(name); comment;) {
      const pugi::xml_node next = comment.next_sibling(name);
      pugi::xml_attribute ref = comment.attribute("ref");
      if (const std::optional<std::string> moved = formula::move_row_addresses(
              ref.value(), edit, edit.sheet, syntax)) {
        if (moved->empty()) {
          list.remove_child(comment);
        } else {
          ref.set_value(moved->c_str());
        }
      }
      comment = next;
    }
  };
  move_refs(comments.child("commentList"), "comment");
  move_refs(threaded, "threadedComment");

  // a note states its cell in `x:Row`, and its box in `x:Anchor` as column,
  // offset, row and offset of two corners
  if (!vml) {
    return;
  }
  std::vector<pugi::xml_node> removed;
  for (const pugi::xpath_node found :
       vml.select_nodes("//*[local-name()='ClientData'][@ObjectType='Note']")) {
    const pugi::xml_node data = found.node();
    pugi::xml_text row = data.child("x:Row").text();
    const std::uint32_t old = row.as_uint();
    const auto rows = edit.span(old, old);
    if (!rows.has_value()) {
      removed.push_back(data.parent());
      continue;
    }
    row.set(rows->first);
    pugi::xml_text anchor = data.child("x:Anchor").text();
    std::vector<std::int64_t> values;
    for (const std::string_view value :
         std::views::split(std::string_view(anchor.get()), ',') |
             std::views::transform([](const auto range) {
               return std::string_view(range.begin(), range.end());
             })) {
      values.push_back(std::strtoll(std::string(value).c_str(), nullptr, 10));
    }
    if (values.size() == 8) {
      values[2] += static_cast<std::int64_t>(rows->first) - old;
      values[6] += static_cast<std::int64_t>(rows->first) - old;
      anchor.set(fmt::format("{}", fmt::join(values, ", ")).c_str());
    }
  }
  for (pugi::xml_node shape : removed) {
    shape.parent().remove_child(shape);
  }
}

} // namespace odr::internal
