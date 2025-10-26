#include <odr/internal/odf/odf_parser.hpp>

#include <odr/internal/common/table_cursor.hpp>
#include <odr/internal/odf/odf_element_registry.hpp>

#include <unordered_map>

#include <pugixml.hpp>

namespace odr::internal::odf {

namespace {

using TreeParser =
    std::function<std::tuple<ExtendedElementIdentifier, pugi::xml_node>(
        ElementRegistry &registry, pugi::xml_node node)>;
using ChildrenParser = std::function<void(ElementRegistry &registry,
                                          ExtendedElementIdentifier parent_id,
                                          pugi::xml_node node)>;

std::tuple<ExtendedElementIdentifier, pugi::xml_node>
parse_any_element_tree(ElementRegistry &registry, pugi::xml_node node);

void parse_any_element_children(ElementRegistry &registry,
                                const ExtendedElementIdentifier parent_id,
                                const pugi::xml_node node) {
  for (pugi::xml_node child_node = node.first_child(); child_node;) {
    if (auto [child_id, next_sibling] =
            parse_any_element_tree(registry, child_node);
        child_id.is_null()) {
      child_node = child_node.next_sibling();
    } else {
      registry.append_child(parent_id, child_id);
      child_node = next_sibling;
    }
  }
}

std::tuple<ExtendedElementIdentifier, pugi::xml_node>
parse_element_tree(ElementRegistry &registry, const ElementType type,
                   const pugi::xml_node node,
                   const ChildrenParser &children_parser) {
  if (!node) {
    return {ExtendedElementIdentifier::null(), pugi::xml_node()};
  }

  const ExtendedElementIdentifier element_id = registry.create_element();
  ElementRegistry::Element &element = registry.element(element_id);
  element.type = type;

  children_parser(registry, element_id, node);

  return {element_id, node.next_sibling()};
}

bool is_text_node(const pugi::xml_node node) {
  if (!node) {
    return false;
  }
  if (node.type() == pugi::node_pcdata) {
    return true;
  }

  const std::string name = node.name();

  if (name == "text:s") {
    return true;
  }
  if (name == "text:tab") {
    return true;
  }

  return false;
}

std::tuple<ExtendedElementIdentifier, pugi::xml_node>
parse_text_element(ElementRegistry &registry, const pugi::xml_node first) {
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
parse_table_row(ElementRegistry &registry, const pugi::xml_node node) {
  if (!node) {
    return {ExtendedElementIdentifier::null(), pugi::xml_node()};
  }

  const ExtendedElementIdentifier element_id = registry.create_element();

  for (const pugi::xml_node cell_node : node.children()) {
    // TODO log warning if repeated
    auto [cell, _] = parse_any_element_tree(registry, cell_node);
    registry.append_child(element_id, cell);
  }

  return {element_id, node.next_sibling()};
}

std::tuple<ExtendedElementIdentifier, pugi::xml_node>
parse_table(ElementRegistry &registry, const pugi::xml_node node) {
  if (!node) {
    return {ExtendedElementIdentifier::null(), pugi::xml_node()};
  }

  const ExtendedElementIdentifier element_id = registry.create_element();

  // TODO inflate table first?

  for (const pugi::xml_node column_node : node.children("table:table-column")) {
    const std::uint32_t repeat =
        column_node.attribute("table:number-columns-repeated").as_uint(1);
    for (std::uint32_t i = 0; i < repeat; ++i) {
      // TODO mark as repeated
      auto [column_id, _] = parse_any_element_tree(registry, column_node);
      registry.append_column(element_id, column_id);
    }
  }

  for (const pugi::xml_node row_node : node.children("table:table-row")) {
    // TODO log warning if repeated
    auto [row_id, _] = parse_any_element_tree(registry, row_node);
    registry.append_child(element_id, row_id);
  }

  return {element_id, node.next_sibling()};
}

std::tuple<ExtendedElementIdentifier, pugi::xml_node>
parse_sheet(ElementRegistry &registry, const pugi::xml_node node) {
  if (!node) {
    return {ExtendedElementIdentifier::null(), pugi::xml_node()};
  }

  const ExtendedElementIdentifier element_id = registry.create_element();
  ElementRegistry::Sheet &sheet = registry.create_sheet_element(element_id);

  TableCursor cursor;

  for (const pugi::xml_node column_node : node.children("table:table-column")) {
    const std::uint32_t columns_repeated =
        column_node.attribute("table:number-columns-repeated").as_uint(1);

    sheet.create_column(cursor.column(), columns_repeated, column_node);

    cursor.add_column(columns_repeated);
  }

  sheet.dimensions.columns = cursor.column();
  cursor = {};

  for (const pugi::xml_node row_node : node.children("table:table-row")) {
    const std::uint32_t rows_repeated =
        row_node.attribute("table:number-rows-repeated").as_uint(1);

    sheet.create_row(cursor.row(), rows_repeated, row_node);

    // TODO covered cells
    for (const pugi::xml_node cell_node :
         row_node.children("table:table-cell")) {
      const std::uint32_t columns_repeated =
          cell_node.attribute("table:number-columns-repeated").as_uint(1);
      const std::uint32_t colspan =
          cell_node.attribute("table:number-columns-spanned").as_uint(1);
      const std::uint32_t rowspan =
          cell_node.attribute("table:number-rows-spanned").as_uint(1);

      sheet.create_cell(cursor.column(), cursor.row(), columns_repeated,
                        rows_repeated, cell_node);

      bool empty = false;
      if (!cell_node.first_child()) {
        empty = true;
      }

      if (!empty) {
        for (std::uint32_t row_repeat = 0; row_repeat < rows_repeated;
             ++row_repeat) {
          for (std::uint32_t column_repeat = 0;
               column_repeat < columns_repeated; ++column_repeat) {
            const ExtendedElementIdentifier cell_id =
                ExtendedElementIdentifier::make_cell(
                    element_id.element_id(), cursor.column() + column_repeat,
                    cursor.row() + row_repeat);
            registry.append_sheet_cell(element_id, cell_id);
            parse_any_element_children(registry, cell_id, cell_node);
          }
        }
      }

      cursor.add_cell(colspan, rowspan, columns_repeated);
    }

    cursor.add_row(rows_repeated);
  }

  sheet.dimensions.rows = cursor.row();

  for (const pugi::xml_node shape_node :
       node.child("table:shapes").children()) {
    if (auto [shape_id, _] = parse_any_element_tree(registry, shape_node);
        !shape_id.is_null()) {
      registry.append_shape(element_id, shape_id);
    }
  }

  return {element_id, node.next_sibling()};
}

void parse_presentation_children(ElementRegistry &registry,
                                 const ExtendedElementIdentifier root_id,
                                 const pugi::xml_node node) {
  for (const pugi::xml_node child_node : node.children("draw:page")) {
    auto [child_id, _] = parse_any_element_tree(registry, child_node);
    registry.append_child(root_id, child_id);
  }
}

void parse_spreadsheet_children(ElementRegistry &registry,
                                const ExtendedElementIdentifier root_id,
                                const pugi::xml_node node) {
  for (const pugi::xml_node child_node : node.children("table:table")) {
    auto [child_id, _] = parse_sheet(registry, child_node);
    registry.append_child(root_id, child_id);
  }
}

void parse_drawing_children(ElementRegistry &registry,
                            const ExtendedElementIdentifier root_id,
                            const pugi::xml_node node) {
  for (const pugi::xml_node child_node : node.children("draw:page")) {
    auto [child_id, _] = parse_any_element_tree(registry, child_node);
    registry.append_child(root_id, child_id);
  }
}

std::tuple<ExtendedElementIdentifier, pugi::xml_node>
parse_any_element_tree(ElementRegistry &registry, const pugi::xml_node node) {
  const auto create_default_tree_parser =
      [](const ElementType type,
         const ChildrenParser &children_parser = parse_any_element_children) {
        return [type, children_parser](ElementRegistry &r,
                                       const pugi::xml_node n) {
          return parse_element_tree(r, type, n, children_parser);
        };
      };

  static std::unordered_map<std::string, TreeParser> parser_table{
      {"office:text", create_default_tree_parser(ElementType::root)},
      {"office:presentation",
       create_default_tree_parser(ElementType::root,
                                  parse_presentation_children)},
      {"office:spreadsheet",
       create_default_tree_parser(ElementType::root,
                                  parse_spreadsheet_children)},
      {"office:drawing",
       create_default_tree_parser(ElementType::root, parse_drawing_children)},
      {"text:p", create_default_tree_parser(ElementType::paragraph)},
      {"text:h", create_default_tree_parser(ElementType::paragraph)},
      {"text:span", create_default_tree_parser(ElementType::span)},
      {"text:s", create_default_tree_parser(ElementType::text)},
      {"text:tab", create_default_tree_parser(ElementType::text)},
      {"text:line-break", create_default_tree_parser(ElementType::line_break)},
      {"text:a", create_default_tree_parser(ElementType::link)},
      {"text:bookmark", create_default_tree_parser(ElementType::bookmark)},
      {"text:bookmark-start",
       create_default_tree_parser(ElementType::bookmark)},
      {"text:list", create_default_tree_parser(ElementType::list)},
      {"text:list-header", create_default_tree_parser(ElementType::list_item)},
      {"text:list-item", create_default_tree_parser(ElementType::list_item)},
      {"text:index-title", create_default_tree_parser(ElementType::group)},
      {"text:table-of-content", create_default_tree_parser(ElementType::group)},
      {"text:illustration-index",
       create_default_tree_parser(ElementType::group)},
      {"text:index-body", create_default_tree_parser(ElementType::group)},
      {"text:soft-page-break",
       create_default_tree_parser(ElementType::page_break)},
      {"text:date", create_default_tree_parser(ElementType::group)},
      {"text:time", create_default_tree_parser(ElementType::group)},
      {"text:section", create_default_tree_parser(ElementType::group)},
      //{"text:page-number", create_tree_parser(ElementType::group)},
      //{"text:page-continuation", create_tree_parser(ElementType::group)},
      {"table:table", parse_table},
      {"table:table-column",
       create_default_tree_parser(ElementType::table_column)},
      {"table:table-row", parse_table_row},
      {"table:table-cell", create_default_tree_parser(ElementType::table_cell)},
      {"table:covered-table-cell",
       create_default_tree_parser(ElementType::table_cell)},
      {"draw:frame", create_default_tree_parser(ElementType::frame)},
      {"draw:image", create_default_tree_parser(ElementType::image)},
      {"draw:rect", create_default_tree_parser(ElementType::rect)},
      {"draw:line", create_default_tree_parser(ElementType::line)},
      {"draw:circle", create_default_tree_parser(ElementType::circle)},
      {"draw:custom-shape",
       create_default_tree_parser(ElementType::custom_shape)},
      {"draw:text-box", create_default_tree_parser(ElementType::group)},
      {"draw:g", create_default_tree_parser(ElementType::frame)},
      {"draw:a", create_default_tree_parser(ElementType::link)},
      {"style:master-page",
       create_default_tree_parser(ElementType::master_page)}};

  if (node.type() == pugi::xml_node_type::node_pcdata) {
    return parse_text_element(registry, node);
  }

  if (const auto constructor_it = parser_table.find(node.name());
      constructor_it != std::end(parser_table)) {
    return constructor_it->second(registry, node);
  }

  return {ExtendedElementIdentifier::null(), pugi::xml_node()};
}

} // namespace

} // namespace odr::internal::odf

namespace odr::internal {

ExtendedElementIdentifier odf::parse_tree(ElementRegistry &registry,
                                          const pugi::xml_node node) {
  auto [root, _] = parse_any_element_tree(registry, node);
  return root;
}

} // namespace odr::internal
