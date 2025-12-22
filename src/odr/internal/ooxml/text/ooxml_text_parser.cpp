#include <odr/internal/ooxml/text/ooxml_text_parser.hpp>

#include <odr/document_element.hpp>
#include <odr/internal/ooxml/text/ooxml_text_element_registry.hpp>

#include <functional>
#include <unordered_map>

#include <pugixml.hpp>

namespace odr::internal::ooxml::text {

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
    if (const auto [child_id, next_sibling] =
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
  element.node = node;

  children_parser(registry, element_id, node);

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
parse_text_element(ElementRegistry &registry, const pugi::xml_node first) {
  if (!first) {
    return {ExtendedElementIdentifier::null(), pugi::xml_node()};
  }

  const ExtendedElementIdentifier element_id = registry.create_element();
  ElementRegistry::Element &element = registry.element(element_id);
  element.type = ElementType::text;
  element.node = first;
  auto &[last] = registry.create_text_element(element_id);

  for (last = first; is_text_node(last.next_sibling());
       last = last.next_sibling()) {
  }

  return {element_id, last.next_sibling()};
}

bool is_list_item(const pugi::xml_node node) {
  return node.child("w:pPr").child("w:numPr");
}

std::int32_t list_level(const pugi::xml_node node) {
  return node.child("w:pPr")
      .child("w:numPr")
      .child("w:ilvl")
      .attribute("w:val")
      .as_int(0);
}

std::tuple<ExtendedElementIdentifier, pugi::xml_node>
parse_list_element(ElementRegistry &registry, pugi::xml_node node) {
  if (!node) {
    return {ExtendedElementIdentifier::null(), pugi::xml_node()};
  }

  const ExtendedElementIdentifier element_id = registry.create_element();
  ElementRegistry::Element &element = registry.element(element_id);
  element.type = ElementType::list;
  element.node = node;

  for (; is_list_item(node); node = node.next_sibling()) {
    const std::int32_t level = list_level(node);

    for (std::int32_t i = 0; i < level; ++i) {
      /* TODO fix lists
      auto list_item_unique = std::make_unique<ListItem>(node);
      auto list_item = list_item_unique.get();
      store.push_back(std::move(list_item_unique));

      base->init_append_child(list_item);
       */

      const ExtendedElementIdentifier nested_id = registry.create_element();
      ElementRegistry::Element &nested_element = registry.element(nested_id);
      nested_element.type = ElementType::list;
      nested_element.node = node;

      registry.append_child(element_id, nested_id);
    }

    const ExtendedElementIdentifier item_id = registry.create_element();
    ElementRegistry::Element &item_element = registry.element(item_id);
    item_element.type = ElementType::list_item;
    item_element.node = node;

    registry.append_child(element_id, item_id);

    auto [child_id, _] = parse_element_tree(registry, ElementType::paragraph,
                                            node, parse_any_element_children);
    registry.append_child(item_id, child_id);
  }

  return {element_id, node.next_sibling()};
}

std::tuple<ExtendedElementIdentifier, pugi::xml_node>
parse_table_row_element(ElementRegistry &registry, const pugi::xml_node node) {
  if (!node) {
    return {ExtendedElementIdentifier::null(), pugi::xml_node()};
  }

  const ExtendedElementIdentifier element_id = registry.create_element();
  ElementRegistry::Element &element = registry.element(element_id);
  element.type = ElementType::table_row;
  element.node = node;

  for (const pugi::xml_node cell_node : node.children("w:tc")) {
    auto [cell_id, _] =
        parse_element_tree(registry, ElementType::table_cell, cell_node,
                           parse_any_element_children);
    registry.append_child(element_id, cell_id);
  }

  return {element_id, node.next_sibling()};
}

std::tuple<ExtendedElementIdentifier, pugi::xml_node>
parse_table_element(ElementRegistry &registry, const pugi::xml_node node) {
  if (!node) {
    return {ExtendedElementIdentifier::null(), pugi::xml_node()};
  }

  const ExtendedElementIdentifier element_id = registry.create_element();
  ElementRegistry::Element &element = registry.element(element_id);
  element.type = ElementType::table_row;
  element.node = node;
  registry.create_table_element(element_id);

  for (const pugi::xml_node column_node :
       node.child("w:tblGrid").children("w:gridCol")) {
    const auto [column_id, _] =
        parse_element_tree(registry, ElementType::table_column, column_node,
                           parse_any_element_children);
    registry.append_column(element_id, column_id);
  }

  for (const pugi::xml_node row_node : node.children("w:tr")) {
    const auto [row_id, _] = parse_element_tree(
        registry, ElementType::table_row, row_node, parse_any_element_children);
    registry.append_child(element_id, row_id);
  }

  return {element_id, node.next_sibling()};
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
      {"w:body", create_default_tree_parser(ElementType::root)},
      {"w:t", parse_text_element},
      {"w:tab", parse_text_element},
      {"w:p", create_default_tree_parser(ElementType::paragraph)},
      {"w:r", create_default_tree_parser(ElementType::span)},
      {"w:bookmarkStart", create_default_tree_parser(ElementType::bookmark)},
      {"w:hyperlink", create_default_tree_parser(ElementType::link)},
      {"w:tbl", parse_table_element},
      {"w:gridCol", create_default_tree_parser(ElementType::table_column)},
      {"w:tr", parse_table_row_element},
      {"w:tc", create_default_tree_parser(ElementType::table_cell)},
      {"w:sdt", create_default_tree_parser(ElementType::group)},
      {"w:sdtContent", create_default_tree_parser(ElementType::group)},
      {"w:drawing", create_default_tree_parser(ElementType::frame)},
      {"wp:anchor", create_default_tree_parser(ElementType::group)},
      {"wp:inline", create_default_tree_parser(ElementType::group)},
      {"a:graphic", create_default_tree_parser(ElementType::group)},
      {"a:graphicData", create_default_tree_parser(ElementType::image)},
  };

  if (is_list_item(node)) {
    return parse_list_element(registry, node);
  }

  if (const auto constructor_it = parser_table.find(node.name());
      constructor_it != std::end(parser_table)) {
    return constructor_it->second(registry, node);
  }

  return {ExtendedElementIdentifier::null(), pugi::xml_node()};
}

} // namespace

} // namespace odr::internal::ooxml::text

namespace odr::internal::ooxml {

ExtendedElementIdentifier text::parse_tree(ElementRegistry &registry,
                                           const pugi::xml_node node) {
  auto [root, _] = parse_any_element_tree(registry, node);
  return root;
}

} // namespace odr::internal::ooxml
