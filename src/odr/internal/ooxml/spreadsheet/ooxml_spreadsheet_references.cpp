#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_references.hpp>

#include <odr/table_position.hpp>

#include <odr/internal/formula/formula_parser.hpp>
#include <odr/internal/formula/formula_writer.hpp>

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <string_view>
#include <utility>

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

} // namespace odr::internal
