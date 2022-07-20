#include <functional>
#include <odr/internal/ooxml/presentation/ooxml_presentation_element.hpp>
#include <odr/internal/ooxml/presentation/ooxml_presentation_parser.hpp>
#include <unordered_map>

namespace odr::internal::ooxml::presentation {

namespace {

std::tuple<abstract::Element *, pugi::xml_node>
parse_element_tree(pugi::xml_node node,
                   std::vector<std::unique_ptr<abstract::Element>> &store);

template <typename Derived>
std::tuple<abstract::Element *, pugi::xml_node> default_parse_element_tree(
    pugi::xml_node node,
    std::vector<std::unique_ptr<abstract::Element>> &store) {
  if (!node) {
    return std::make_tuple(nullptr, pugi::xml_node());
  }

  auto element_unique = std::make_unique<Derived>(node);
  auto element = dynamic_cast<abstract::Element *>(element_unique.get());
  store.push_back(std::move(element_unique));

  for (auto child_node : node) {
    auto [child, _] = parse_element_tree(child_node, store);
    if (child == nullptr) {
      continue;
    }

    // TODO attach child to root
  }

  return std::make_tuple(element, node.next_sibling());
}

std::tuple<abstract::Element *, pugi::xml_node>
parse_element_tree(pugi::xml_node node,
                   std::vector<std::unique_ptr<abstract::Element>> &store) {
  using Parser = std::function<std::tuple<abstract::Element *, pugi::xml_node>(
      pugi::xml_node node,
      std::vector<std::unique_ptr<abstract::Element>> & store)>;

  using Group = DefaultElement<ElementType::group>;

  static std::unordered_map<std::string, Parser> parser_table{
      {"p:presentation", default_parse_element_tree<Root>},
      {"p:sld", default_parse_element_tree<Slide>},
      {"p:sp", default_parse_element_tree<Frame>},
      {"p:txBody", default_parse_element_tree<Group>},
      {"a:t", default_parse_element_tree<Text>},
      {"a:p", default_parse_element_tree<Paragraph>},
      {"a:r", default_parse_element_tree<Span>},
      {"a:tbl", default_parse_element_tree<TableElement>},
      {"a:gridCol", default_parse_element_tree<TableColumn>},
      {"a:tr", default_parse_element_tree<TableRow>},
      {"a:tc", default_parse_element_tree<TableCell>},
  };

  if (auto constructor_it = parser_table.find(node.name());
      constructor_it != std::end(parser_table)) {
    return constructor_it->second(node, store);
  }

  return std::make_tuple(nullptr, pugi::xml_node());
}

} // namespace

} // namespace odr::internal::ooxml::presentation

namespace odr::internal::ooxml {

std::tuple<abstract::Element *, std::vector<std::unique_ptr<abstract::Element>>>
presentation::parse_tree(pugi::xml_node node) {
  std::vector<std::unique_ptr<abstract::Element>> store;
  auto [root, _] = parse_element_tree(node, store);
  return std::make_tuple(root, std::move(store));
}

} // namespace odr::internal::ooxml
