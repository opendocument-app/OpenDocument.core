#include <functional>
#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_element.hpp>
#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_parser.hpp>
#include <unordered_map>

namespace odr::internal::ooxml::spreadsheet {

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
  for (; is_text_node(last.next_sibling()); last = last.next_sibling()) {
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

  static std::unordered_map<std::string, Parser> parser_table{
      {"workbook", parse_element_tree<Root>},
      {"worksheet", parse_element_tree<Sheet>},
      {"col", parse_element_tree<TableColumn>},
      {"row", parse_element_tree<TableRow>},
      {"r", parse_element_tree<Span>},
      {"t", parse_element_tree<Text>},
      {"v", parse_element_tree<Text>},
      {"xdr:twoCellAnchor", parse_element_tree<Frame>},
  };

  if (auto constructor_it = parser_table.find(node.name());
      constructor_it != std::end(parser_table)) {
    return constructor_it->second(node, store);
  }

  return std::make_tuple(nullptr, pugi::xml_node());
}

} // namespace

} // namespace odr::internal::ooxml::spreadsheet

namespace odr::internal::ooxml {

std::tuple<spreadsheet::Element *,
           std::vector<std::unique_ptr<spreadsheet::Element>>>
spreadsheet::parse_tree(pugi::xml_node node) {
  std::vector<std::unique_ptr<spreadsheet::Element>> store;
  auto [root, _] = parse_any_element_tree(node, store);
  return std::make_tuple(root, std::move(store));
}

} // namespace odr::internal::ooxml
