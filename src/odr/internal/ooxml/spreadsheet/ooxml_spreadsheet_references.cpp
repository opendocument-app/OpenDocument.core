#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_references.hpp>

#include <odr/table_position.hpp>

#include <odr/internal/common/table_range.hpp>
#include <odr/internal/formula/formula_parser.hpp>
#include <odr/internal/formula/formula_writer.hpp>
#include <odr/internal/util/string_util.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <fmt/format.h>
#include <fmt/ranges.h>

namespace odr::internal::ooxml::spreadsheet {

namespace {

constexpr formula::Syntax syntax = formula::Syntax::ooxml;

std::optional<TablePosition> moved_position(const TablePosition &position,
                                            const bool edited,
                                            const formula::SheetEdit &edit) {
  return edited ? move_position(position, edit) : position;
}

/// @p node as the cell at @p to reads it, where the one at @p from states it.
formula::Node shifted(formula::Node node, const TablePosition &from,
                      const TablePosition &to) {
  formula::shift(node, static_cast<std::int64_t>(to.column) - from.column,
                 static_cast<std::int64_t>(to.row) - from.row);
  return node;
}

void move_ref(pugi::xml_node formula, const std::string &sheet,
              const formula::SheetEdit &edit) {
  if (pugi::xml_attribute ref = formula.attribute("ref")) {
    if (const std::optional<std::string> moved =
            formula::move_addresses(ref.value(), edit, sheet, syntax)) {
      ref.set_value(moved->c_str());
    }
  }
}

/// Moves the expression @p node states, where it parses.
void move_text(pugi::xml_node node, const std::optional<std::string> &sheet,
               const formula::SheetEdit &edit) {
  if (std::optional<formula::Node> expression =
          formula::parse(node.text().get(), syntax);
      expression.has_value() &&
      formula::move_references(*expression, edit, sheet)) {
    node.text().set(formula::to_string(*expression, syntax).c_str());
  }
}

/// Moves @p formulas of a rule or a validation of the edited sheet, which
/// state their cells as the first cell of @p sqref sees them. Where a delete
/// removes that cell, they read as the first cell that stays.
void move_anchored_formulas(const std::string &sqref,
                            const std::span<const pugi::xml_node> formulas,
                            const formula::SheetEdit &edit) {
  const std::string first = sqref.substr(0, sqref.find_first_of(" :"));
  if (first.empty()) {
    return;
  }
  const TablePosition anchor(first);
  TablePosition stays = anchor;
  if (!edit.insert && !move_position(anchor, edit).has_value()) {
    const std::uint32_t after = edit.index + edit.count;
    stays = edit.axis == formula::Axis::row
                ? TablePosition(anchor.column, after)
                : TablePosition(after, anchor.row);
  }
  for (const pugi::xml_node text : formulas) {
    if (stays == anchor) {
      move_text(text, edit.sheet, edit);
    } else if (const std::optional<formula::Node> expression =
                   formula::parse(text.text().get(), syntax)) {
      formula::Node moved = shifted(*expression, anchor, stays);
      formula::move_references(moved, edit, edit.sheet);
      text.text().set(formula::to_string(moved, syntax).c_str());
    }
  }
}

void move_rule_formulas(const pugi::xml_node node,
                        const formula::SheetEdit &edit) {
  std::vector<pugi::xml_node> formulas;
  for (const pugi::xpath_node match :
       node.select_nodes("cfRule/formula | formula1 | formula2")) {
    formulas.push_back(match.node());
  }
  move_anchored_formulas(node.attribute("sqref").value(), formulas, edit);
}

/// The descendants of @p node named @p name.
std::vector<pugi::xml_node> descendants(const pugi::xml_node node,
                                        const std::string_view name) {
  std::vector<pugi::xml_node> result;
  for (pugi::xml_node child : node.children()) {
    if (child.name() == name) {
      result.push_back(child);
    }
    const std::vector<pugi::xml_node> inner = descendants(child, name);
    result.insert(result.end(), inner.begin(), inner.end());
  }
  return result;
}

/// Removes @p node, and every parent it leaves without an element up to the
/// worksheet. A parent stating `count` counts the entries that stay.
void remove_entry(pugi::xml_node node) {
  while (node && std::string_view(node.name()) != "worksheet") {
    pugi::xml_node parent = node.parent();
    parent.remove_child(node);
    if (parent.find_child([](const pugi::xml_node child) {
          return child.type() == pugi::node_element;
        })) {
      if (pugi::xml_attribute count = parent.attribute("count")) {
        count.set_value(static_cast<std::uint32_t>(std::ranges::count_if(
            parent.children(), [](const pugi::xml_node child) {
              return child.type() == pugi::node_element;
            })));
      }
      return;
    }
    node = parent;
  }
}

/// [MS-XLSX] 2.3: the Excel 2010 extensions of a worksheet state their cells
/// in `xm:sqref` and their formulas in `xm:f`, which may read another sheet.
/// So each worksheet moves its formulas, and the edited one its cells too.
void move_extensions(const NamedWorksheet &worksheet, const bool edited,
                     const formula::SheetEdit &edit) {
  const pugi::xml_node extensions = worksheet.node.child("extLst");
  std::vector<pugi::xml_node> lost;
  // a rule or a validation reads as the first cell of its range
  for (const std::string_view name :
       {"x14:conditionalFormatting", "x14:dataValidation"}) {
    for (const pugi::xml_node rule : descendants(extensions, name)) {
      const std::vector<pugi::xml_node> formulas = descendants(rule, "xm:f");
      const pugi::xml_node sqref = rule.child("xm:sqref");
      if (!edited) {
        for (const pugi::xml_node text : formulas) {
          move_text(text, worksheet.name, edit);
        }
        continue;
      }
      move_anchored_formulas(sqref.text().get(), formulas, edit);
      if (const std::optional<std::string> moved = formula::move_addresses(
              sqref.text().get(), edit, edit.sheet, syntax)) {
        if (moved->empty()) {
          lost.push_back(rule);
        } else {
          sqref.text().set(moved->c_str());
        }
      }
    }
  }
  // a sparkline reads its range as it is, and sits in its own cell
  for (const pugi::xml_node sparkline :
       descendants(extensions, "x14:sparkline")) {
    move_text(sparkline.child("xm:f"), worksheet.name, edit);
    const pugi::xml_node sqref = sparkline.child("xm:sqref");
    if (!edited) {
      continue;
    }
    if (const std::optional<std::string> moved = formula::move_addresses(
            sqref.text().get(), edit, edit.sheet, syntax)) {
      if (moved->empty()) {
        lost.push_back(sparkline);
      } else {
        sqref.text().set(moved->c_str());
      }
    }
  }
  for (const pugi::xml_node node : lost) {
    remove_entry(node);
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
                       const formula::SheetEdit &edit) {
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
  const bool master_moved = formula::move_references(moved_master, edit, sheet);
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
    formula::move_references(expected, edit, sheet);
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
                    const formula::SheetEdit &edit) {
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
  move_extensions(worksheet, edited, edit);
}

/// ECMA-376 18.6.1: an entry without `i` is on the sheet of the one before.
void move_calc_chain(pugi::xml_node calc_chain,
                     const std::string &edited_sheet_id,
                     const formula::SheetEdit &edit) {
  std::string sheet_id;
  for (pugi::xml_node entry = calc_chain.first_child(); entry;) {
    pugi::xml_node next = entry.next_sibling();
    if (const pugi::xml_attribute id = entry.attribute("i")) {
      sheet_id = id.value();
    }
    if (sheet_id == edited_sheet_id) {
      if (const std::optional<TablePosition> at = move_position(
              TablePosition(entry.attribute("r").value()), edit)) {
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
/// rows or columns where a delete takes it.
std::string moved_cell(const std::string &address,
                       const formula::SheetEdit &edit) {
  const TablePosition position(address);
  if (const std::optional<TablePosition> moved =
          move_position(position, edit)) {
    return moved->to_string();
  }
  return (edit.axis == formula::Axis::row
              ? TablePosition(position.column, edit.index)
              : TablePosition(edit.index, position.row))
      .to_string();
}

/// A `filterColumn` counts its `colId` from the filter's first column, so a
/// column edit moves it, and removes one whose column it takes.
void move_filter_columns(pugi::xml_node filter,
                         const formula::SheetEdit &edit) {
  const std::string ref = filter.attribute("ref").value();
  if (ref.empty()) {
    return;
  }
  const TableRange range(ref.contains(':') ? ref : ref + ":" + ref);
  const auto columns = edit.span(range.from().column, range.to().column);
  if (!columns.has_value()) {
    return; // the filter goes as a whole
  }
  for (pugi::xml_node column = filter.child("filterColumn"); column;) {
    const pugi::xml_node next = column.next_sibling("filterColumn");
    const std::uint32_t at =
        range.from().column + column.attribute("colId").as_uint();
    if (const auto moved = edit.span(at, at)) {
      column.attribute("colId").set_value(moved->first - columns->first);
    } else {
      filter.remove_child(column);
    }
    column = next;
  }
}

/// Moves the `ref` of @p node, and removes the node where the edit takes it.
void move_range(pugi::xml_node node, const formula::SheetEdit &edit) {
  pugi::xml_attribute ref = node.attribute("ref");
  if (!ref) {
    return;
  }
  if (const std::optional<std::string> moved =
          formula::move_addresses(ref.value(), edit, edit.sheet, syntax)) {
    if (moved->empty()) {
      node.parent().remove_child(node);
    } else {
      ref.set_value(moved->c_str());
    }
  }
}

/// Whether the edit removes all of @p ref, a range list of the edited sheet.
bool removes_all(const std::string_view ref, const formula::SheetEdit &edit) {
  const std::optional<std::string> moved =
      formula::move_addresses(ref, edit, edit.sheet, syntax);
  return moved.has_value() && moved->empty();
}

/// The `worksheetSource` of @p cache where it reads the edited sheet, else
/// null. One with an `r:id` reads another workbook.
pugi::xml_node edited_source(const pugi::xml_node cache,
                             const formula::SheetEdit &edit) {
  const pugi::xml_node source =
      cache.child("cacheSource").child("worksheetSource");
  return source.attribute("ref") && !source.attribute("r:id") &&
                 source.attribute("sheet").value() == edit.sheet
             ? source
             : pugi::xml_node();
}

void collect_ranges(const pugi::xml_node node, const formula::SheetEdit &edit,
                    std::vector<pugi::xml_node> &lost) {
  for (pugi::xml_node child : node.children()) {
    const std::string_view name = child.name();
    if (child.type() != pugi::node_element || name == "sheetData") {
      continue;
    }
    if (name == "autoFilter" && edit.axis == formula::Axis::column) {
      move_filter_columns(child, edit);
    }
    if (name == "conditionalFormatting" || name == "dataValidation") {
      move_rule_formulas(child, edit);
    }
    for (const RangeAttribute &range : removable_ranges) {
      pugi::xml_attribute attribute = child.attribute(range.attribute.data());
      if (name != range.element || !attribute) {
        continue;
      }
      if (const std::optional<std::string> moved = formula::move_addresses(
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
        if (const std::optional<std::string> moved = formula::move_addresses(
                sqref.value(), edit, edit.sheet, syntax)) {
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

/// The child of an anchor corner stating the edit's axis.
const char *corner_axis(const formula::SheetEdit &edit) {
  return edit.axis == formula::Axis::row ? "xdr:row" : "xdr:col";
}

/// The row or column of an anchor corner after the edit, at the edge of the
/// ones that stay where a delete takes it.
void move_corner(pugi::xml_node corner, const formula::SheetEdit &edit) {
  pugi::xml_text at = corner.child(corner_axis(edit)).text();
  if (const auto span = edit.span(at.as_uint(), at.as_uint())) {
    at.set(span->first);
  } else {
    at.set(edit.index);
    corner.child(edit.axis == formula::Axis::row ? "xdr:rowOff" : "xdr:colOff")
        .text()
        .set(0);
  }
}

} // namespace

} // namespace odr::internal::ooxml::spreadsheet

namespace odr::internal {

void ooxml::spreadsheet::move_workbook_references(
    const pugi::xml_node workbook,
    const std::vector<NamedWorksheet> &worksheets,
    const pugi::xml_node calc_chain, const std::string &edited_sheet_id,
    const formula::SheetEdit &edit) {
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
                                           const formula::SheetEdit &edit) {
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

void ooxml::spreadsheet::move_breaks(const pugi::xml_node worksheet,
                                     const formula::SheetEdit &edit) {
  pugi::xml_node breaks = worksheet.child(
      edit.axis == formula::Axis::row ? "rowBreaks" : "colBreaks");
  std::vector<std::uint32_t> seen;
  for (pugi::xml_node brk = breaks.child("brk"); brk;) {
    const pugi::xml_node next = brk.next_sibling("brk");
    const std::uint32_t id = brk.attribute("id").as_uint();
    const auto span = edit.span(id, id);
    const std::uint32_t moved = span.has_value() ? span->first : edit.index;
    if (std::ranges::find(seen, moved) != seen.end()) {
      breaks.remove_child(brk);
    } else {
      seen.push_back(moved);
      brk.attribute("id").set_value(moved);
    }
    brk = next;
  }
  if (pugi::xml_attribute count = breaks.attribute("count")) {
    count.set_value(static_cast<std::uint32_t>(seen.size()));
  }
  if (pugi::xml_attribute manual = breaks.attribute("manualBreakCount")) {
    manual.set_value(static_cast<std::uint32_t>(std::ranges::count_if(
        breaks.children("brk"), [](const pugi::xml_node brk) {
          return brk.attribute("man").as_bool();
        })));
  }
}

void ooxml::spreadsheet::move_drawing(const pugi::xml_node drawing,
                                      const formula::SheetEdit &edit) {
  for (const pugi::xml_node anchor : drawing.children()) {
    const std::string_view edit_as = anchor.attribute("editAs").value();
    const pugi::xml_node from = anchor.child("xdr:from");
    if (!from || edit_as == "absolute") {
      continue;
    }
    const pugi::xml_text from_at = from.child(corner_axis(edit)).text();
    const std::int64_t old = from_at.as_uint();
    move_corner(from, edit);
    const pugi::xml_node to = anchor.child("xdr:to");
    if (!to) {
      continue;
    }
    if (edit_as != "oneCell") {
      move_corner(to, edit);
    } else {
      // a `oneCell` box keeps its size, so its far corner moves as the first
      pugi::xml_text to_at = to.child(corner_axis(edit)).text();
      to_at.set(static_cast<std::uint32_t>(std::max<std::int64_t>(
          0, to_at.as_llong() + from_at.as_llong() - old)));
    }
  }
}

void ooxml::spreadsheet::move_comments(const pugi::xml_node comments,
                                       const pugi::xml_node threaded,
                                       const pugi::xml_node vml,
                                       const formula::SheetEdit &edit) {
  const auto move_refs = [&](pugi::xml_node list, const char *name) {
    for (pugi::xml_node comment = list.child(name); comment;) {
      const pugi::xml_node next = comment.next_sibling(name);
      pugi::xml_attribute ref = comment.attribute("ref");
      if (const std::optional<std::string> moved =
              formula::move_addresses(ref.value(), edit, edit.sheet, syntax)) {
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

  // a note states its cell in `x:Row` and `x:Column`, and its box in
  // `x:Anchor` as column, offset, row and offset of two corners
  if (!vml) {
    return;
  }
  const bool rows = edit.axis == formula::Axis::row;
  const std::size_t first = rows ? 2 : 0;
  std::vector<pugi::xml_node> removed;
  for (const pugi::xpath_node found :
       vml.select_nodes("//*[local-name()='ClientData'][@ObjectType='Note']")) {
    const pugi::xml_node data = found.node();
    pugi::xml_text at = data.child(rows ? "x:Row" : "x:Column").text();
    const std::uint32_t old = at.as_uint();
    const auto span = edit.span(old, old);
    if (!span.has_value()) {
      removed.push_back(data.parent());
      continue;
    }
    at.set(span->first);
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
      values[first] += static_cast<std::int64_t>(span->first) - old;
      values[first + 4] += static_cast<std::int64_t>(span->first) - old;
      anchor.set(fmt::format("{}", fmt::join(values, ", ")).c_str());
    }
  }
  for (pugi::xml_node shape : removed) {
    shape.parent().remove_child(shape);
  }
}

std::optional<TablePosition>
ooxml::spreadsheet::move_position(const TablePosition &position,
                                  const formula::SheetEdit &edit) {
  const bool rows = edit.axis == formula::Axis::row;
  const std::uint32_t along = rows ? position.row : position.column;
  const auto span = edit.span(along, along);
  if (!span.has_value()) {
    return std::nullopt;
  }
  return rows ? TablePosition(position.column, span->first)
              : TablePosition(span->first, position.row);
}

namespace {

/// The range @p table covers, header and totals rows included.
TableRange table_range(const pugi::xml_node table) {
  const std::string ref = table.attribute("ref").value();
  return TableRange(ref.contains(':') ? ref : ref + ":" + ref);
}

} // namespace

bool ooxml::spreadsheet::cuts_table(const pugi::xml_node table,
                                    const formula::SheetEdit &edit) {
  const TableRange range = table_range(table);
  const auto removed = [&](const std::uint32_t first,
                           const std::uint32_t last) {
    return !edit.span(first, last).has_value();
  };
  if (edit.axis == formula::Axis::column) {
    return removed(range.from().column, range.to().column);
  }
  const std::uint32_t top = range.from().row;
  const std::uint32_t bottom = range.to().row;
  return removed(top, bottom) ||
         (table.attribute("headerRowCount").as_uint(1) > 0 &&
          removed(top, top)) ||
         (table.attribute("totalsRowCount").as_uint(0) > 0 &&
          removed(bottom, bottom));
}

std::vector<ooxml::spreadsheet::TableHeader>
ooxml::spreadsheet::move_table(pugi::xml_node table,
                               const formula::SheetEdit &edit) {
  std::vector<TableHeader> result;
  const TableRange range = table_range(table);

  pugi::xml_node columns = table.child("tableColumns");
  if (edit.axis == formula::Axis::column) {
    const std::uint32_t first = range.from().column;
    const std::uint32_t last = range.to().column;
    if (edit.insert && first < edit.index && edit.index <= last) {
      std::uint32_t id = 0;
      std::set<std::string> names; // excel compares them ignoring case
      for (const pugi::xml_node column : columns.children("tableColumn")) {
        id = std::max(id, column.attribute("id").as_uint());
        names.insert(util::string::to_lower(column.attribute("name").value()));
      }
      pugi::xml_node next = columns.child("tableColumn");
      for (std::uint32_t at = first; at < edit.index; ++at) {
        next = next.next_sibling("tableColumn");
      }
      std::uint32_t suffix = 1;
      for (std::uint32_t i = 0; i < edit.count; ++i) {
        std::string name;
        do {
          name = "Column" + std::to_string(suffix++);
        } while (!names.insert(util::string::to_lower(name)).second);
        pugi::xml_node column =
            next ? columns.insert_child_before("tableColumn", next)
                 : columns.append_child("tableColumn");
        column.append_attribute("id").set_value(++id);
        column.append_attribute("name").set_value(name.c_str());
        if (table.attribute("headerRowCount").as_uint(1) > 0) {
          result.push_back(
              {TablePosition(edit.index + i, range.from().row), name});
        }
      }
    } else if (!edit.insert) {
      std::uint32_t at = first;
      for (pugi::xml_node column = columns.child("tableColumn"); column; ++at) {
        const pugi::xml_node next = column.next_sibling("tableColumn");
        if (!edit.span(at, at).has_value()) {
          columns.remove_child(column);
        }
        column = next;
      }
    }
    if (pugi::xml_attribute count = columns.attribute("count")) {
      count.set_value(static_cast<std::uint32_t>(
          std::ranges::distance(columns.children("tableColumn"))));
    }
  }
  for (const pugi::xml_node column : columns.children("tableColumn")) {
    for (const char *name : {"calculatedColumnFormula", "totalsRowFormula"}) {
      if (const pugi::xml_node formula = column.child(name)) {
        move_text(formula, edit.sheet, edit);
      }
    }
  }
  const pugi::xml_node filter = table.child("autoFilter");
  if (filter && edit.axis == formula::Axis::column) {
    move_filter_columns(filter, edit);
  }
  for (const pugi::xml_node sort :
       {filter.child("sortState"), table.child("sortState")}) {
    for (pugi::xml_node condition = sort.child("sortCondition"); condition;) {
      const pugi::xml_node next = condition.next_sibling("sortCondition");
      move_range(condition, edit);
      condition = next;
    }
    move_range(sort, edit);
  }
  move_range(filter, edit);
  move_range(table, edit);
  return result;
}

void ooxml::spreadsheet::move_chart(const pugi::xml_node chart,
                                    const formula::SheetEdit &edit) {
  for (const pugi::xpath_node formula :
       chart.select_nodes("//*[local-name()='f']")) {
    move_text(formula.node(), std::nullopt, edit);
  }
}

bool ooxml::spreadsheet::cuts(const std::string &ref,
                              const formula::SheetEdit &edit) {
  const TableRange range(ref.contains(':') ? ref : ref + ":" + ref);
  const bool rows = edit.axis == formula::Axis::row;
  const std::uint32_t first = rows ? range.from().row : range.from().column;
  const std::uint32_t last = rows ? range.to().row : range.to().column;
  if (edit.insert) {
    return first < edit.index && edit.index <= last;
  }
  const std::uint64_t end = std::uint64_t{edit.index} + edit.count;
  return first < end && last >= edit.index &&
         (first < edit.index || last >= end);
}

bool ooxml::spreadsheet::cuts_pivot(const pugi::xml_node pivot,
                                    const formula::SheetEdit &edit) {
  const std::string ref = pivot.child("location").attribute("ref").value();
  return !ref.empty() && (cuts(ref, edit) || removes_all(ref, edit));
}

void ooxml::spreadsheet::move_pivot(const pugi::xml_node pivot,
                                    const formula::SheetEdit &edit) {
  move_range(pivot.child("location"), edit);
}

bool ooxml::spreadsheet::loses_pivot_source(const pugi::xml_node cache,
                                            const formula::SheetEdit &edit) {
  const pugi::xml_node source = edited_source(cache, edit);
  return source && removes_all(source.attribute("ref").value(), edit);
}

void ooxml::spreadsheet::move_pivot_cache(const pugi::xml_node cache,
                                          const formula::SheetEdit &edit) {
  move_range(edited_source(cache, edit), edit);
}

} // namespace odr::internal
