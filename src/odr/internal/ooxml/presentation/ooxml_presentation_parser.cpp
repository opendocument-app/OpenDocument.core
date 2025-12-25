#include <odr/internal/ooxml/presentation/ooxml_presentation_parser.hpp>

#include <odr/document_element.hpp>
#include <odr/internal/ooxml/presentation/ooxml_presentation_element_registry.hpp>

#include <functional>
#include <unordered_map>

#include <pugixml.hpp>

namespace odr::internal::ooxml::presentation {

namespace {

using TreeParser = std::function<std::tuple<ElementIdentifier, pugi::xml_node>(
    ElementRegistry &registry, pugi::xml_node node)>;
using ChildrenParser =
    std::function<void(ElementRegistry &registry, ElementIdentifier parent_id,
                       pugi::xml_node node)>;

std::tuple<ElementIdentifier, pugi::xml_node>
parse_any_element_tree(ElementRegistry &registry, pugi::xml_node node);

void parse_any_element_children(ElementRegistry &registry,
                                const ElementIdentifier parent_id,
                                const pugi::xml_node node) {
  for (pugi::xml_node child_node = node.first_child(); child_node;) {
    if (const auto [child_id, next_sibling] =
            parse_any_element_tree(registry, child_node);
        child_id == null_element_id) {
      child_node = child_node.next_sibling();
    } else {
      registry.append_child(parent_id, child_id);
      child_node = next_sibling;
    }
  }
}

std::tuple<ElementIdentifier, pugi::xml_node>
parse_element_tree(ElementRegistry &registry, const ElementType type,
                   const pugi::xml_node node,
                   const ChildrenParser &children_parser) {
  if (!node) {
    return {null_element_id, pugi::xml_node()};
  }

  const ElementIdentifier element_id = registry.create_element();
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

std::tuple<ElementIdentifier, pugi::xml_node>
parse_text_element(ElementRegistry &registry, const pugi::xml_node first) {
  if (!first) {
    return {null_element_id, pugi::xml_node()};
  }

  const ElementIdentifier element_id = registry.create_element();
  ElementRegistry::Element &element = registry.element(element_id);
  element.type = ElementType::text;
  element.node = first;
  auto &[last] = registry.create_text_element(element_id);

  for (last = first; is_text_node(last.next_sibling());
       last = last.next_sibling()) {
  }

  return {element_id, last.next_sibling()};
}

std::tuple<ElementIdentifier, pugi::xml_node>
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
      {"p:presentation", create_default_tree_parser(ElementType::root)},
      {"p:sld", create_default_tree_parser(ElementType::slide)},
      {"p:sp", create_default_tree_parser(ElementType::frame)},
      {"p:txBody", create_default_tree_parser(ElementType::group)},
      {"a:t", parse_text_element},
      {"a:p", create_default_tree_parser(ElementType::paragraph)},
      {"a:r", create_default_tree_parser(ElementType::span)},
      {"a:tbl", create_default_tree_parser(ElementType::table)},
      {"a:gridCol", create_default_tree_parser(ElementType::table_column)},
      {"a:tr", create_default_tree_parser(ElementType::table_row)},
      {"a:tc", create_default_tree_parser(ElementType::table_cell)},
  };

  if (auto constructor_it = parser_table.find(node.name());
      constructor_it != std::end(parser_table)) {
    return constructor_it->second(registry, node);
  }

  return {null_element_id, pugi::xml_node()};
}

} // namespace

} // namespace odr::internal::ooxml::presentation

namespace odr::internal::ooxml {

ElementIdentifier presentation::parse_tree(ElementRegistry &registry,
                                           const pugi::xml_node node) {
  auto [root, _] = parse_any_element_tree(registry, node);
  return root;
}

} // namespace odr::internal::ooxml
