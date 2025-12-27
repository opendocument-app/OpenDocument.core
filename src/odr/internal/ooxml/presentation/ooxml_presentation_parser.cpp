#include <odr/internal/ooxml/presentation/ooxml_presentation_parser.hpp>

#include <odr/document_element.hpp>
#include <odr/internal/ooxml/presentation/ooxml_presentation_element_registry.hpp>

#include <functional>
#include <unordered_map>

namespace odr::internal::ooxml::presentation {

namespace {

class Context {
public:
  Context(ElementRegistry &registry,
          const std::unordered_map<std::string, pugi::xml_document> &slides_xml)
      : m_registry(&registry), m_slides_xml(&slides_xml) {}

  ElementRegistry &registry() const { return *m_registry; }
  const std::unordered_map<std::string, pugi::xml_document> &
  slides_xml() const {
    return *m_slides_xml;
  }

private:
  ElementRegistry *m_registry{nullptr};
  const std::unordered_map<std::string, pugi::xml_document> *m_slides_xml{
      nullptr};
};

using TreeParser = std::function<std::tuple<ElementIdentifier, pugi::xml_node>(
    const Context &context, pugi::xml_node node)>;
using ChildrenParser = std::function<void(
    const Context &context, ElementIdentifier parent_id, pugi::xml_node node)>;

std::tuple<ElementIdentifier, pugi::xml_node>
parse_any_element_tree(const Context &context, pugi::xml_node node);

void parse_any_element_children(const Context &context,
                                const ElementIdentifier parent_id,
                                const pugi::xml_node node) {
  for (pugi::xml_node child_node = node.first_child(); child_node;) {
    if (const auto [child_id, next_sibling] =
            parse_any_element_tree(context, child_node);
        child_id == null_element_id) {
      child_node = child_node.next_sibling();
    } else {
      context.registry().append_child(parent_id, child_id);
      child_node = next_sibling;
    }
  }
}

std::tuple<ElementIdentifier, pugi::xml_node>
parse_element_tree(const Context &context, const ElementType type,
                   const pugi::xml_node node,
                   const ChildrenParser &children_parser) {
  if (!node) {
    return {null_element_id, pugi::xml_node()};
  }

  const auto &[element_id, _] = context.registry().create_element(type, node);

  children_parser(context, element_id, node);

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
parse_text_element(const Context &context, const pugi::xml_node first) {
  if (!first) {
    return {null_element_id, pugi::xml_node()};
  }

  pugi::xml_node last;
  for (last = first; is_text_node(last.next_sibling());
       last = last.next_sibling()) {
  }

  const auto &[element_id, _, __] =
      context.registry().create_text_element(first, last);

  return {element_id, last.next_sibling()};
}

void parse_slide_children(const Context &context,
                          const ElementIdentifier parent_id,
                          const pugi::xml_node node) {
  parse_any_element_children(context, parent_id,
                             node.child("p:cSld").child("p:spTree"));
}

void parse_presentation_children(const Context &context,
                                 const ElementIdentifier root_id,
                                 const pugi::xml_node node) {
  for (const pugi::xml_node child_node :
       node.child("p:sldIdLst").children("p:sldId")) {
    const std::string id = child_node.attribute("r:id").value();
    const pugi::xml_node slide_node =
        context.slides_xml().at(id).document_element();
    auto [child_id, _] = parse_element_tree(context, ElementType::slide,
                                            slide_node, parse_slide_children);
    context.registry().append_child(root_id, child_id);
  }
}

std::tuple<ElementIdentifier, pugi::xml_node>
parse_any_element_tree(const Context &context, const pugi::xml_node node) {
  const auto create_default_tree_parser =
      [](const ElementType type,
         const ChildrenParser &children_parser = parse_any_element_children) {
        return
            [type, children_parser](const Context &c, const pugi::xml_node n) {
              return parse_element_tree(c, type, n, children_parser);
            };
      };

  static std::unordered_map<std::string, TreeParser> parser_table{
      {"p:presentation", create_default_tree_parser(
                             ElementType::root, parse_presentation_children)},
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

  if (const auto constructor_it = parser_table.find(node.name());
      constructor_it != std::end(parser_table)) {
    return constructor_it->second(context, node);
  }

  return {null_element_id, pugi::xml_node()};
}

} // namespace

} // namespace odr::internal::ooxml::presentation

namespace odr::internal::ooxml {

ElementIdentifier presentation::parse_tree(
    ElementRegistry &registry, const pugi::xml_node node,
    const std::unordered_map<std::string, pugi::xml_document> &slides_xml) {
  const Context context(registry, slides_xml);
  auto [root, _] = parse_any_element_tree(context, node);
  return root;
}

} // namespace odr::internal::ooxml
