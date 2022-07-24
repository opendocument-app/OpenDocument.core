#include <functional>
#include <odr/internal/ooxml/text/ooxml_text_element.hpp>
#include <odr/internal/ooxml/text/ooxml_text_parser.hpp>
#include <unordered_map>

namespace odr::internal::ooxml::text {

namespace {

template <typename Derived>
std::tuple<Element *, pugi::xml_node>
parse_element_tree(pugi::xml_node node,
                   std::vector<std::unique_ptr<Element>> &store);

std::tuple<Element *, pugi::xml_node>
parse_any_element_tree(pugi::xml_node node,
                       std::vector<std::unique_ptr<Element>> &store);

void parse_element_children(Element *element, pugi::xml_node node,
                            std::vector<std::unique_ptr<Element>> &store) {
  for (auto child_node = node.first_child(); child_node;) {
    auto [child, next_sibling] = parse_any_element_tree(child_node, store);
    if (child == nullptr) {
      child_node = child_node.next_sibling();
    } else {
      element->init_append_child(child);
      child_node = next_sibling;
    }
  }
}

template <typename Derived>
std::tuple<Element *, pugi::xml_node>
parse_element_tree(pugi::xml_node node,
                   std::vector<std::unique_ptr<Element>> &store) {
  if (!node) {
    return std::make_tuple(nullptr, pugi::xml_node());
  }

  auto element_unique = std::make_unique<Derived>(node);
  auto element = element_unique.get();
  store.push_back(std::move(element_unique));

  parse_element_children(element, node, store);

  return std::make_tuple(element, node.next_sibling());
}

bool is_text_node(const pugi::xml_node node) {
  if (!node) {
    return false;
  }

  std::string name = node.name();

  if (name == "w:t") {
    return true;
  }
  if (name == "w:tab") {
    return true;
  }

  return false;
}

template <>
std::tuple<Element *, pugi::xml_node>
parse_element_tree<Text>(pugi::xml_node first,
                         std::vector<std::unique_ptr<Element>> &store) {
  if (!first) {
    return std::make_tuple(nullptr, pugi::xml_node());
  }

  pugi::xml_node last = first;
  for (; is_text_node(last); last = last.next_sibling()) {
  }

  auto element_unique = std::make_unique<Text>(first, last);
  auto element = element_unique.get();
  store.push_back(std::move(element_unique));

  return std::make_tuple(element, last.next_sibling());
}

std::tuple<Element *, pugi::xml_node>
parse_any_element_tree(pugi::xml_node node,
                       std::vector<std::unique_ptr<Element>> &store) {
  using Parser = std::function<std::tuple<Element *, pugi::xml_node>(
      pugi::xml_node node, std::vector<std::unique_ptr<Element>> & store)>;

  using Group = DefaultElement<ElementType::group>;

  static std::unordered_map<std::string, Parser> parser_table{
      {"w:body", parse_element_tree<Root>},
      {"w:t", parse_element_tree<Text>},
      {"w:tab", parse_element_tree<Text>},
      {"w:p", parse_element_tree<Paragraph>},
      {"w:r", parse_element_tree<Span>},
      {"w:bookmarkStart", parse_element_tree<Bookmark>},
      {"w:hyperlink", parse_element_tree<Link>},
      {"w:tbl", parse_element_tree<TableElement>},
      {"w:gridCol", parse_element_tree<TableColumn>},
      {"w:tr", parse_element_tree<TableRow>},
      {"w:tc", parse_element_tree<TableCell>},
      {"w:sdt", parse_element_tree<Group>},
      {"w:sdtContent", parse_element_tree<Group>},
      {"w:drawing", parse_element_tree<Frame>},
      {"wp:anchor", parse_element_tree<Group>},
      {"a:graphicData", parse_element_tree<ImageElement>},
  };

  if (ListElement::is_list_item(node)) {
    return parse_element_tree<ListRoot>(node, store);
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

std::tuple<text::Element *, std::vector<std::unique_ptr<text::Element>>>
text::parse_tree(pugi::xml_node node) {
  std::vector<std::unique_ptr<text::Element>> store;
  auto [root, _] = parse_any_element_tree(node, store);
  return std::make_tuple(root, std::move(store));
}

} // namespace odr::internal::ooxml
