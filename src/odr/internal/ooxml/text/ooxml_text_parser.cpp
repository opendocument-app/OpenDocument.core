#include <functional>
#include <odr/internal/ooxml/text/ooxml_text_element.hpp>
#include <odr/internal/ooxml/text/ooxml_text_parser.hpp>
#include <unordered_map>

namespace odr::internal::ooxml::text {

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
      {"w:body", default_parse_element_tree<Root>},
      {"w:t", default_parse_element_tree<Text>},
      {"w:tab", default_parse_element_tree<Text>},
      {"w:p", default_parse_element_tree<Paragraph>},
      {"w:r", default_parse_element_tree<Span>},
      {"w:bookmarkStart", default_parse_element_tree<Bookmark>},
      {"w:hyperlink", default_parse_element_tree<Link>},
      {"w:tbl", default_parse_element_tree<TableElement>},
      {"w:gridCol", default_parse_element_tree<TableColumn>},
      {"w:tr", default_parse_element_tree<TableRow>},
      {"w:tc", default_parse_element_tree<TableCell>},
      {"w:sdt", default_parse_element_tree<Group>},
      {"w:sdtContent", default_parse_element_tree<Group>},
      {"w:drawing", default_parse_element_tree<Frame>},
      {"wp:anchor", default_parse_element_tree<Group>},
      {"a:graphicData", default_parse_element_tree<ImageElement>},
  };

  if (ListElement::is_list_item(node)) {
    return default_parse_element_tree<ListRoot>(node, store);
  }

  if (auto constructor_it = parser_table.find(node.name());
      constructor_it != std::end(parser_table)) {
    return constructor_it->second(node, store);
  }

  return std::make_tuple(nullptr, pugi::xml_node());
}

} // namespace

} // namespace odr::internal::ooxml::text

namespace odr::internal::ooxml {

std::tuple<abstract::Element *, std::vector<std::unique_ptr<abstract::Element>>>
text::parse_tree(pugi::xml_node node) {
  std::vector<std::unique_ptr<abstract::Element>> store;
  auto [root, _] = parse_element_tree(node, store);
  return std::make_tuple(root, store);
}

} // namespace odr::internal::ooxml
