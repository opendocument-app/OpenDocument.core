#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_parser.hpp>

#include <odr/document_element.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/common/table_range.hpp>
#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_element_registry.hpp>

#include <unordered_map>

#include <pugixml.hpp>

namespace odr::internal::ooxml::spreadsheet {

namespace {

using TreeParser =
    std::function<std::tuple<ExtendedElementIdentifier, pugi::xml_node>(
        ElementRegistry &registry, const ParseContext &context,
        pugi::xml_node node)>;
using ChildrenParser = std::function<void(
    ElementRegistry &registry, const ParseContext &context,
    ExtendedElementIdentifier parent_id, pugi::xml_node node)>;

std::tuple<ExtendedElementIdentifier, pugi::xml_node>
parse_any_element_tree(ElementRegistry &registry, const ParseContext &context,
                       pugi::xml_node node);

void parse_any_element_children(ElementRegistry &registry,
                                const ParseContext &context,
                                const ExtendedElementIdentifier parent_id,
                                const pugi::xml_node node) {
  for (pugi::xml_node child_node = node.first_child(); child_node;) {
    if (const auto [child_id, next_sibling] =
            parse_any_element_tree(registry, context, child_node);
        child_id.is_null()) {
      child_node = child_node.next_sibling();
    } else {
      registry.append_child(parent_id, child_id);
      child_node = next_sibling;
    }
  }
}

std::tuple<ExtendedElementIdentifier, pugi::xml_node>
parse_element_tree(ElementRegistry &registry, const ParseContext &context,
                   const ElementType type, const pugi::xml_node node,
                   const ChildrenParser &children_parser) {
  if (!node) {
    return {ExtendedElementIdentifier::null(), pugi::xml_node()};
  }

  const ExtendedElementIdentifier element_id = registry.create_element();
  ElementRegistry::Element &element = registry.element(element_id);
  element.type = type;

  children_parser(registry, context, element_id, node);

  return {element_id, node.next_sibling()};
}

void parse_root_children(ElementRegistry &registry, const ParseContext &context,
                         ExtendedElementIdentifier parent_id,
                         const pugi::xml_node node) {
  for (pugi::xml_node child_node : node.child("sheets").children("sheet")) {
    const char *id = child_node.attribute("r:id").value();
    AbsPath sheet_path = context.get_document_path().parent().join(
        RelPath(context.get_document_relations().at(id)));
    const auto &[sheet_xml, sheet_relations] =
        context.get_documents_and_relations().at(sheet_path);
    ParseContext newContext(sheet_path, sheet_relations,
                            context.get_documents_and_relations(),
                            context.get_shared_strings());
    const auto &[sheet, _] = parse_any_element_tree(
        registry, newContext, sheet_xml.document_element());
    registry.append_child(parent_id, sheet);
  }
}

void parse_sheet_cell_children(ElementRegistry &registry,
                               const ParseContext &context,
                               ExtendedElementIdentifier parent_id,
                               const pugi::xml_node node) {
  if (const pugi::xml_attribute type_attr = node.attribute("t");
      type_attr.value() == std::string("s")) {
    const pugi::xml_node v_node = node.child("v");
    const std::size_t ref = v_node.first_child().text().as_ullong();
    const pugi::xml_node shared_node = context.get_shared_strings().at(ref);
    parse_any_element_children(registry, context, parent_id, shared_node);
    return;
  }

  parse_any_element_children(registry, context, parent_id, node);
}

void parse_frame_children(ElementRegistry &registry,
                          const ParseContext &context,
                          ExtendedElementIdentifier parent_id,
                          const pugi::xml_node node) {
  if (const pugi::xml_node image_node =
          node.child("xdr:pic").child("xdr:blipFill").child("a:blip")) {
    auto [image, _] = parse_any_element_tree(registry, context, image_node);
    registry.append_child(parent_id, image);
  }
}

std::tuple<ExtendedElementIdentifier, pugi::xml_node>
parse_sheet_element(ElementRegistry &registry, const ParseContext &context,
                    const pugi::xml_node node) {
  if (!node) {
    return {ExtendedElementIdentifier::null(), pugi::xml_node()};
  }

  const ExtendedElementIdentifier element_id = registry.create_element();
  ElementRegistry::Element &element = registry.element(element_id);
  element.type = ElementType::sheet;
  ElementRegistry::Sheet &sheet = registry.create_sheet_element(element_id);

  for (const pugi::xml_node col_node : node.child("cols").children("col")) {
    const std::uint32_t min = col_node.attribute("min").as_uint() - 1;
    const std::uint32_t max = col_node.attribute("max").as_uint() - 1;
    sheet.create_column(min, max - min, col_node);
  }

  for (const pugi::xml_node row_node :
       node.child("sheetData").children("row")) {
    const std::uint32_t row = row_node.attribute("r").as_uint() - 1;
    sheet.create_row(row, row_node);

    for (const pugi::xml_node cell_node : row_node.children("c")) {
      TablePosition position(cell_node.attribute("r").value());
      sheet.create_cell(position.column(), position.row(), cell_node);

      auto [cell, _] = parse_any_element_tree(registry, context, cell_node);
      registry.append_sheet_cell(element_id, cell);
    }
  }

  {
    const std::string dimension_ref =
        node.child("dimension").attribute("ref").value();
    TablePosition position_to;
    if (dimension_ref.find(':') == std::string::npos) {
      position_to = TablePosition(dimension_ref);
    } else {
      position_to = TableRange(dimension_ref).to();
    }
    sheet.dimensions =
        TableDimensions(position_to.row() + 1, position_to.column() + 1);
  }

  if (const pugi::xml_node drawing_node = node.child("drawing")) {
    const char *id = drawing_node.attribute("r:id").value();
    const AbsPath drawing_path = context.get_document_path().parent().join(
        RelPath(context.get_document_relations().at(id)));
    const auto &[drawing_xml, drawing_relations] =
        context.get_documents_and_relations().at(drawing_path);

    for (const pugi::xml_node shape_node :
         drawing_xml.document_element().children()) {
      auto [shape, _] = parse_any_element_tree(registry, context, shape_node);
      if (!shape.is_null()) {
        registry.append_shape(element_id, shape);
      }
    }
  }

  return {element_id, node.next_sibling()};
}

bool is_text_node(const pugi::xml_node node) {
  if (!node) {
    return false;
  }

  const std::string name = node.name();

  if (name == "w:t") {
    return true;
  }
  if (name == "w:tab") {
    return true;
  }

  return false;
}

std::tuple<ExtendedElementIdentifier, pugi::xml_node>
parse_text_element(ElementRegistry &registry, const ParseContext &context,
                   const pugi::xml_node first) {
  if (!first) {
    return {ExtendedElementIdentifier::null(), pugi::xml_node()};
  }

  const ExtendedElementIdentifier element_id = registry.create_element();
  auto &[last] = registry.create_text_element(element_id);

  for (last = first; is_text_node(last.next_sibling());
       last = last.next_sibling()) {
  }

  return {element_id, last.next_sibling()};
}

std::tuple<ExtendedElementIdentifier, pugi::xml_node>
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
      {"xdr:twoCellAnchor",
       create_default_tree_parser(ElementType::frame, parse_frame_children)},
  };

  if (const auto constructor_it = parser_table.find(node.name());
      constructor_it != std::end(parser_table)) {
    return constructor_it->second(registry, context, node);
  }

  return {ExtendedElementIdentifier::null(), pugi::xml_node()};
}

} // namespace

} // namespace odr::internal::ooxml::spreadsheet

namespace odr::internal::ooxml {

ExtendedElementIdentifier spreadsheet::parse_tree(ElementRegistry &registry,
                                                  const ParseContext &context,
                                                  const pugi::xml_node node) {
  auto [root, _] = parse_any_element_tree(registry, context, node);
  return root;
}

} // namespace odr::internal::ooxml
