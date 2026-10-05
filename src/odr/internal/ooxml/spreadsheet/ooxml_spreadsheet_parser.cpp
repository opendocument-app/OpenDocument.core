#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_parser.hpp>

#include <odr/document_element.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/common/table_range.hpp>
#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_element_registry.hpp>
#include <odr/internal/util/string_util.hpp>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

#include <pugixml.hpp>

namespace odr::internal::ooxml::spreadsheet {

namespace {

std::optional<std::uint32_t> read_index(std::string_view text) {
  text = util::string::trim_view(text);
  if (text.starts_with('+')) {
    text.remove_prefix(1);
  }
  std::uint32_t result = 0;
  const auto [end, error] =
      std::from_chars(text.data(), text.data() + text.size(), result);
  if (error != std::errc{} || end != text.data() + text.size()) {
    return std::nullopt;
  }
  return result;
}

using TreeParser = std::function<std::tuple<ElementIdentifier, pugi::xml_node>(
    ElementRegistry &registry, const ParseContext &context,
    pugi::xml_node node)>;
using ChildrenParser =
    std::function<void(ElementRegistry &registry, const ParseContext &context,
                       ElementIdentifier parent_id, pugi::xml_node node)>;

std::tuple<ElementIdentifier, pugi::xml_node>
parse_any_element_tree(ElementRegistry &registry, const ParseContext &context,
                       pugi::xml_node node);

void parse_any_element_children(ElementRegistry &registry,
                                const ParseContext &context,
                                const ElementIdentifier parent_id,
                                const pugi::xml_node node) {
  for (pugi::xml_node child_node = node.first_child(); child_node;) {
    const auto [child_id, next_sibling] =
        parse_any_element_tree(registry, context, child_node);
    if (child_id == null_element_id) {
      child_node = child_node.next_sibling();
      continue;
    }

    registry.append_child(parent_id, child_id);
    child_node = next_sibling;
  }
}

std::tuple<ElementIdentifier, pugi::xml_node>
parse_element_tree(ElementRegistry &registry, const ParseContext &context,
                   const ElementType type, const pugi::xml_node node,
                   const ChildrenParser &children_parser) {
  if (!node) {
    return {null_element_id, pugi::xml_node()};
  }

  const auto &[element_id, _] = registry.create_element(type, node);

  children_parser(registry, context, element_id, node);

  return {element_id, node.next_sibling()};
}

void parse_root_children(ElementRegistry &registry, const ParseContext &context,
                         const ElementIdentifier parent_id,
                         const pugi::xml_node node) {
  for (pugi::xml_node child_node : node.child("sheets").children("sheet")) {
    const char *id = child_node.attribute("r:id").value();
    const AbsPath sheet_path = resolve_part_path(
        context.document_path(), context.document_relations().at(id));
    const auto &[sheet_xml, sheet_relations] =
        context.documents_and_relations().at(sheet_path);
    const ParseContext sheet_context(sheet_path, sheet_relations,
                                     context.documents_and_relations(),
                                     context.shared_strings());
    const auto &[sheet, _] = parse_any_element_tree(
        registry, sheet_context, sheet_xml.document_element());
    registry.sheet_element_at(sheet).name =
        child_node.attribute("name").value();
    registry.append_child(parent_id, sheet);
  }
}

void parse_sheet_cell_children(ElementRegistry &registry,
                               const ParseContext &context,
                               const ElementIdentifier parent_id,
                               const pugi::xml_node node) {
  const std::string_view type = node.attribute("t").value();

  // ECMA-376 18.3.1.4: a shared string indexes `sharedStrings.xml`, an inline
  // one carries the same content model under `is`. Both hold the text one
  // level below the cell, where the walker does not descend on its own.
  if (type == "s") {
    // Like LibreOffice, an unknown shared string leaves the cell empty.
    const std::optional<std::uint32_t> ref =
        read_index(node.child("v").text().get());
    if (ref && *ref < context.shared_strings().size()) {
      parse_any_element_children(registry, context, parent_id,
                                 context.shared_strings()[*ref]);
    }
    return;
  }
  if (type == "inlineStr") {
    parse_any_element_children(registry, context, parent_id, node.child("is"));
    return;
  }

  parse_any_element_children(registry, context, parent_id, node);
}

std::tuple<ElementIdentifier, pugi::xml_node>
parse_sheet_element(ElementRegistry &registry, const ParseContext &context,
                    const pugi::xml_node node) {
  if (!node) {
    return {null_element_id, pugi::xml_node()};
  }

  const auto &[element_id, _, sheet] = registry.create_sheet_element(node);
  registry.attach_element_relations(element_id, context.document_relations(),
                                    context.document_path());

  for (const pugi::xml_node col_node : node.child("cols").children("col")) {
    // Like LibreOffice, an invalid range drops only its `col`.
    const std::optional<std::uint32_t> min =
        read_index(col_node.attribute("min").value());
    const std::optional<std::uint32_t> max =
        read_index(col_node.attribute("max").value());
    if (!min || !max || *min == 0 || *max < *min) {
      continue;
    }
    sheet.register_column(*min - 1, *max - 1, col_node);
  }

  TableDimensions used;
  // State inferred coordinates so later edits read the same positions.
  std::uint64_t next_row = 1;
  for (pugi::xml_node row_node : node.child("sheetData").children("row")) {
    // An invalid `r` counts as omitted.
    pugi::xml_attribute row_attribute = row_node.attribute("r");
    const std::optional<std::uint32_t> stated =
        row_attribute ? read_index(row_attribute.value()) : std::nullopt;
    const std::uint64_t row_number =
        stated && *stated != 0 ? *stated : next_row;
    if (row_number > std::numeric_limits<std::uint32_t>::max()) {
      throw std::runtime_error("invalid spreadsheet row");
    }
    const std::uint32_t row = static_cast<std::uint32_t>(row_number - 1);
    next_row = row_number + 1;
    if (!stated || *stated != row_number) {
      if (!row_attribute) {
        row_attribute = row_node.append_attribute("r");
      }
      row_attribute.set_value(static_cast<std::uint32_t>(row_number));
    }
    sheet.register_row(row, row_node);

    std::uint32_t next_column = 0;
    for (pugi::xml_node cell_node : row_node.children("c")) {
      const pugi::xml_attribute reference = cell_node.attribute("r");
      if (!reference &&
          next_column == std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("invalid spreadsheet column");
      }
      const TablePosition position = reference
                                         ? TablePosition(reference.value())
                                         : TablePosition(next_column, row);
      next_column = position.column + 1;
      if (!reference) {
        cell_node.append_attribute("r").set_value(position.to_string().c_str());
      }

      const auto &[cell_id, unused1, unused2] =
          registry.create_sheet_cell_element(cell_node, position);
      registry.append_sheet_cell(element_id, cell_id);
      sheet.register_cell(position.column, position.row, cell_node, cell_id);
      parse_sheet_cell_children(registry, context, cell_id, cell_node);

      // [ECMA-376] 18.3.1.40: only the master spells the expression
      if (const pugi::xml_node formula_node = cell_node.child("f");
          std::string_view(formula_node.attribute("t").value()) == "shared" &&
          !std::string_view(formula_node.text().get()).empty()) {
        sheet.shared_formulas.emplace(formula_node.attribute("si").value(),
                                      ElementRegistry::Sheet::SharedFormula{
                                          position, formula_node.text().get()});
      }

      used.rows = std::max(used.rows, position.row + 1);
      used.columns = std::max(used.columns, position.column + 1);
    }
  }

  for (const pugi::xml_node merge_node :
       node.child("mergeCells").children("mergeCell")) {
    const std::string ref = merge_node.attribute("ref").value();
    if (ref.find(':') == std::string::npos) {
      continue;
    }
    const TableRange range(ref);

    // covered flags are only meaningful next to the anchor's span, so a range
    // whose anchor is absent from sheetData must be dropped entirely
    const ElementRegistry::Sheet::Cell *anchor =
        sheet.cell(range.from().column, range.from().row);
    if (anchor == nullptr) {
      continue;
    }
    registry.sheet_cell_element_at(anchor->element_id).span =
        TableDimensions(range.to().row - range.from().row + 1,
                        range.to().column - range.from().column + 1);

    // A `ref` may name the whole grid, 17 billion positions - so past the
    // point where the range is bigger than the sheet has cells, walk the cells.
    const std::uint64_t area =
        static_cast<std::uint64_t>(range.to().row - range.from().row + 1) *
        (range.to().column - range.from().column + 1);

    if (area > sheet.cells.size()) {
      for (const auto &[position, cell] : sheet.cells) {
        if (position != range.from() && range.contains(position)) {
          registry.sheet_cell_element_at(cell.element_id).is_covered = true;
        }
      }
      continue;
    }

    for (std::uint32_t row = range.from().row; row <= range.to().row; ++row) {
      for (std::uint32_t column = range.from().column;
           column <= range.to().column; ++column) {
        if (row == range.from().row && column == range.from().column) {
          continue;
        }
        if (const ElementRegistry::Sheet::Cell *cell = sheet.cell(column, row);
            cell != nullptr) {
          registry.sheet_cell_element_at(cell->element_id).is_covered = true;
        }
      }
    }
  }

  // `dimension` is optional; fall back to the range the cells actually span
  if (const std::string dimension_ref =
          node.child("dimension").attribute("ref").value();
      dimension_ref.empty()) {
    sheet.dimensions = used;
  } else {
    const TablePosition position_to =
        dimension_ref.find(':') == std::string::npos
            ? TablePosition(dimension_ref)
            : TableRange(dimension_ref).to();
    sheet.dimensions =
        TableDimensions(position_to.row + 1, position_to.column + 1);
  }

  if (const pugi::xml_node drawing_node = node.child("drawing")) {
    const char *id = drawing_node.attribute("r:id").value();
    const AbsPath drawing_path = resolve_part_path(
        context.document_path(), context.document_relations().at(id));
    const auto &[drawing_xml, drawing_relations] =
        context.documents_and_relations().at(drawing_path);

    const ParseContext drawing_context(drawing_path, drawing_relations,
                                       context.documents_and_relations(),
                                       context.shared_strings());

    for (const pugi::xml_node shape_node :
         drawing_xml.document_element().children()) {
      const auto [shape, _] =
          parse_any_element_tree(registry, drawing_context, shape_node);
      if (shape == null_element_id) {
        continue;
      }
      registry.append_shape(element_id, shape);
    }
  }

  return {element_id, node.next_sibling()};
}

bool is_text_node(const pugi::xml_node node) {
  if (!node) {
    return false;
  }

  const std::string name = node.name();

  if (name == "t") {
    return true;
  }
  if (name == "v") {
    return true;
  }

  return false;
}

std::tuple<ElementIdentifier, pugi::xml_node>
parse_text_element(ElementRegistry &registry,
                   [[maybe_unused]] const ParseContext &context,
                   const pugi::xml_node first) {
  if (!first) {
    return {null_element_id, pugi::xml_node()};
  }

  pugi::xml_node last;
  for (last = first; is_text_node(last.next_sibling());
       last = last.next_sibling()) {
  }

  const auto &[element_id, unused1, unused2] =
      registry.create_text_element(first, last);

  return {element_id, last.next_sibling()};
}

std::tuple<ElementIdentifier, pugi::xml_node>
parse_frame_element(ElementRegistry &registry, const ParseContext &context,
                    const pugi::xml_node node) {
  if (!node) {
    return {null_element_id, pugi::xml_node()};
  }

  const auto &[element_id, _] =
      registry.create_element(ElementType::frame, node);
  registry.attach_element_relations(element_id, context.document_relations(),
                                    context.document_path());

  if (const pugi::xml_node image_node =
          node.child("xdr:pic").child("xdr:blipFill").child("a:blip")) {
    auto [image, _] =
        parse_element_tree(registry, context, ElementType::image, image_node,
                           parse_any_element_children);
    registry.append_child(element_id, image);
  }

  return {element_id, node.next_sibling()};
}

std::tuple<ElementIdentifier, pugi::xml_node>
parse_any_element_tree(ElementRegistry &registry, const ParseContext &context,
                       const pugi::xml_node node) {
  const auto create_default_tree_parser =
      [](const ElementType type,
         const ChildrenParser &children_parser = parse_any_element_children) {
        return
            [type, children_parser](ElementRegistry &r, const ParseContext &c,
                                    const pugi::xml_node n) {
              return parse_element_tree(r, c, type, n, children_parser);
            };
      };

  static std::unordered_map<std::string, TreeParser> parser_table{
      {"workbook",
       create_default_tree_parser(ElementType::root, parse_root_children)},
      {"worksheet", parse_sheet_element},
      {"c", create_default_tree_parser(ElementType::sheet_cell,
                                       parse_sheet_cell_children)},
      {"r", create_default_tree_parser(ElementType::span)},
      {"t", parse_text_element},
      {"v", parse_text_element},
      {"xdr:twoCellAnchor", parse_frame_element}};

  if (const auto constructor_it = parser_table.find(node.name());
      constructor_it != std::end(parser_table)) {
    return constructor_it->second(registry, context, node);
  }

  return {null_element_id, pugi::xml_node()};
}

} // namespace

} // namespace odr::internal::ooxml::spreadsheet

namespace odr::internal::ooxml {

ElementIdentifier spreadsheet::parse_tree(ElementRegistry &registry,
                                          const ParseContext &context,
                                          const pugi::xml_node node) {
  auto [root, _] = parse_any_element_tree(registry, context, node);
  return root;
}

} // namespace odr::internal::ooxml
