#include <odr/internal/odf/odf_sheet_references.hpp>

#include <odr/internal/formula/formula_ast.hpp>
#include <odr/internal/formula/formula_dependencies.hpp>
#include <odr/internal/formula/formula_parser.hpp>
#include <odr/internal/formula/formula_writer.hpp>
#include <odr/internal/odf/odf_table.hpp>

#include <algorithm>
#include <array>
#include <limits>
#include <optional>
#include <string_view>

#include <pugixml.hpp>

namespace odr::internal::odf {

namespace {

/// The attributes stating a cell or range address, or a list of them
/// ([ODF 1.2] 9.2.5, 18.584, 19.587).
constexpr std::array<std::string_view, 11> address_attributes{
    "table:cell-range-address",
    "table:base-cell-address",
    "table:target-range-address",
    "table:end-cell-address",
    "table:print-ranges",
    "calcext:target-range-address",
    "calcext:base-cell-address",
    "style:base-cell-address",
    "draw:notify-on-update-of-ranges",
    "chart:values-cell-range-address",
    "chart:label-cell-address"};

/// Whether the edit can change what @p node computes: it reads a removed row
/// or column, or a range an insert grows.
bool touched(const formula::Node &node, const std::string &sheet,
             const formula::SheetEdit &edit) {
  const std::uint64_t end = static_cast<std::uint64_t>(edit.index) + edit.count;
  for (const formula::Extent &extent : formula::references(node).extents) {
    if (extent.document.has_value() ||
        extent.sheet.value_or(sheet) != edit.sheet) {
      continue;
    }
    const bool rows = edit.axis == formula::Axis::row;
    const std::uint32_t first =
        rows ? extent.range.from().row : extent.range.from().column;
    const std::uint32_t last =
        rows ? extent.range.to().row : extent.range.to().column;
    if (edit.insert ? first < edit.index && edit.index <= last &&
                          last != std::numeric_limits<std::uint32_t>::max()
                    : first < end && last >= edit.index) {
      return true;
    }
  }
  return false;
}

/// Moves the formula in @p attribute, keeping the namespace prefix it states.
/// True where the edit can change its result.
bool move_formula(pugi::xml_attribute attribute, const std::string &sheet,
                  const formula::SheetEdit &edit) {
  const std::string_view text = attribute.value();
  std::optional<formula::Node> node =
      formula::parse(text, formula::Syntax::opendocument);
  if (!node.has_value()) {
    return false;
  }
  const bool result = touched(*node, sheet, edit);
  if (formula::move_references(*node, edit, sheet)) {
    const std::string_view prefix =
        text.substr(0, text.size() - formula::strip_prefix(text).size());
    attribute.set_value(
        (std::string(prefix) +
         formula::to_string(*node, formula::Syntax::opendocument))
            .c_str());
  }
  return result;
}

/// The attributes stating a condition, whose references sit in brackets
/// inside an expression that is no formula: `formula-is([.A1]>5)`,
/// `of:cell-content-is-between(1;[.B1])`, `cell-content()>[.C1]`.
constexpr std::array<std::string_view, 3> condition_attributes{
    "calcext:value", "table:condition", "style:condition"};

/// The attributes stating the cell a condition is relative to, whose sheet
/// is the one a reference of the condition names by stating none.
constexpr std::array<std::string_view, 3> base_cell_attributes{
    "table:base-cell-address", "calcext:base-cell-address",
    "style:base-cell-address"};

/// @p text with every bracketed reference in it moved. Nothing where none
/// moved. A string literal is left alone, and so is a bracket inside a quoted
/// sheet name.
std::optional<std::string> move_bracketed(const std::string_view text,
                                          const std::string &sheet,
                                          const formula::SheetEdit &edit) {
  std::string result;
  bool moved = false;
  for (std::size_t i = 0; i < text.size();) {
    if (text[i] == '"') {
      const std::size_t end = text.find('"', i + 1);
      const std::size_t stop =
          end == std::string_view::npos ? text.size() : end + 1;
      result += text.substr(i, stop - i);
      i = stop;
      continue;
    }
    if (text[i] != '[') {
      result += text[i++];
      continue;
    }
    std::size_t end = i + 1;
    for (bool quoted = false; end < text.size(); ++end) {
      if (text[end] == '\'') {
        quoted = !quoted;
      } else if (!quoted && text[end] == ']') {
        break;
      }
    }
    const std::string_view reference = text.substr(i, end + 1 - i);
    if (std::optional<formula::Node> node =
            formula::parse(reference, formula::Syntax::opendocument);
        end < text.size() && node.has_value() &&
        formula::move_references(*node, edit, sheet)) {
      result += formula::to_string(*node, formula::Syntax::opendocument);
      moved = true;
    } else {
      result += reference;
    }
    i = end + 1;
  }
  if (!moved) {
    return std::nullopt;
  }
  return result;
}

/// The sheet @p address names, empty where it names none.
std::string sheet_of(const std::string_view address) {
  const std::optional<formula::Node> node = formula::parse(
      "[" + std::string(address) + "]", formula::Syntax::opendocument);
  if (!node.has_value() || !node->holds<formula::CellReference>()) {
    return {};
  }
  return node->get<formula::CellReference>().sheet.value_or("");
}

/// Moves the named expressions and the addresses @p node and its subtree
/// state, the unstated sheet being the table they sit in. A cell's formula is
/// left to the caller.
void move_subtree(const pugi::xml_node node, std::string sheet,
                  const formula::SheetEdit &edit) {
  if (std::string_view(node.name()) == "table:table") {
    sheet = node.attribute("table:name").value();
  }
  // a condition reads relative to its base cell, whose sheet is its own
  std::string own = sheet;
  for (const std::string_view base : base_cell_attributes) {
    if (const pugi::xml_attribute address = node.attribute(base.data())) {
      if (const std::string named = sheet_of(address.value()); !named.empty()) {
        own = named;
      }
    }
  }
  for (pugi::xml_attribute attribute : node.attributes()) {
    const std::string_view name = attribute.name();
    if (name == "table:expression") {
      move_formula(attribute, sheet, edit);
    } else if (std::ranges::find(condition_attributes, name) !=
               condition_attributes.end()) {
      if (const std::optional<std::string> moved =
              move_bracketed(attribute.value(), own, edit)) {
        attribute.set_value(moved->c_str());
      }
    } else if (std::ranges::find(address_attributes, name) !=
               address_attributes.end()) {
      if (const std::optional<std::string> moved = formula::move_addresses(
              attribute.value(), edit, sheet, formula::Syntax::opendocument)) {
        attribute.set_value(moved->empty() ? "#REF!" : moved->c_str());
      }
    }
  }
  for (const pugi::xml_node child : node.children()) {
    if (child.type() == pugi::node_element) {
      move_subtree(child, sheet, edit);
    }
  }
}

} // namespace

} // namespace odr::internal::odf

namespace odr::internal {

std::vector<SheetPosition>
odf::move_sheet_references(const pugi::xml_node spreadsheet,
                           const formula::SheetEdit &edit) {
  std::vector<SheetPosition> result;
  const bool rows = edit.axis == formula::Axis::row;

  move_subtree(spreadsheet, "", edit);
  // a cell style's `style:map` conditions live with the automatic styles
  move_subtree(spreadsheet.parent().parent().child("office:automatic-styles"),
               "", edit);

  std::uint32_t ordinal = 0;
  for (const pugi::xml_node table : spreadsheet.children("table:table")) {
    const std::string sheet = table.attribute("table:name").value();
    const bool edited = sheet == edit.sheet;

    std::uint32_t row_begin = 0;
    for_each_table_row(table, [&](const pugi::xml_node row_node) {
      const std::uint32_t row_end =
          row_begin +
          row_node.attribute("table:number-rows-repeated").as_uint(1);
      std::uint32_t column_begin = 0;
      for (const pugi::xml_node cell : row_node.children()) {
        const std::uint32_t column_end =
            column_begin +
            cell.attribute("table:number-columns-repeated").as_uint(1);
        const pugi::xml_attribute formula = cell.attribute("table:formula");
        if (formula && move_formula(formula, sheet, edit)) {
          for (std::uint32_t row = row_begin; row < row_end; ++row) {
            for (std::uint32_t column = column_begin; column < column_end;
                 ++column) {
              // where the cell itself sits after the edit
              const std::uint32_t along = rows ? row : column;
              if (!edited) {
                result.emplace_back(ordinal, column, row);
              } else if (const auto moved = edit.span(along, along)) {
                result.emplace_back(ordinal, rows ? column : moved->first,
                                    rows ? moved->first : row);
              }
            }
          }
        }
        column_begin = column_end;
      }
      row_begin = row_end;
    });
    ++ordinal;
  }

  return result;
}

void odf::move_object_references(const pugi::xml_node object,
                                 const formula::SheetEdit &edit) {
  move_subtree(object, "", edit);
}

} // namespace odr::internal
