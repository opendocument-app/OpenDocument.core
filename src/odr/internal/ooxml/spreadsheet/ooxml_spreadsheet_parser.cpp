#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_parser.hpp>

#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_document.hpp>

#include <functional>
#include <unordered_map>

namespace odr::internal::ooxml::spreadsheet {

namespace {

template <typename element_t>
std::tuple<Element *, pugi::xml_node> parse_element_tree(Document &document,
                                                         pugi::xml_node node);

std::tuple<Element *, pugi::xml_node>
parse_any_element_tree(Document &document, pugi::xml_node node);

void parse_element_children(Document &document, Element *element,
                            pugi::xml_node node) {
  for (auto child_node = node.first_child(); child_node;) {
    auto [child, next_sibling] = parse_any_element_tree(document, child_node);
    if (child == nullptr) {
      child_node = child_node.next_sibling();
    } else {
      element->append_child_(child);
      child_node = next_sibling;
    }
  }
}

template <typename element_t>
std::tuple<Element *, pugi::xml_node> parse_element_tree(Document &document,
                                                         pugi::xml_node node) {
  if (!node) {
    return std::make_tuple(nullptr, pugi::xml_node());
  }

  auto element_unique = std::make_unique<element_t>(node);
  auto element = element_unique.get();
  document.register_element_(std::move(element_unique));

  parse_element_children(document, element, node);

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
parse_element_tree<Text>(Document &document, pugi::xml_node first) {
  if (!first) {
    return std::make_tuple(nullptr, pugi::xml_node());
  }

  pugi::xml_node last = first;
  for (; is_text_node(last.next_sibling()); last = last.next_sibling()) {
  }

  auto element_unique = std::make_unique<Text>(first, last);
  auto element = element_unique.get();
  document.register_element_(std::move(element_unique));

  return std::make_tuple(element, last.next_sibling());
}

std::tuple<Element *, pugi::xml_node>
parse_any_element_tree(Document &document, pugi::xml_node node) {
  using Parser = std::function<std::tuple<Element *, pugi::xml_node>(
      Document & document, pugi::xml_node node)>;

  static std::unordered_map<std::string, Parser> parser_table{
      {"workbook", parse_element_tree<Root>},
      {"worksheet", parse_element_tree<Sheet>},
      // TODO
      //{"col", parse_element_tree<SheetColumn>},
      //{"row", parse_element_tree<SheetRow>},
      {"r", parse_element_tree<Span>},
      {"t", parse_element_tree<Text>},
      {"v", parse_element_tree<Text>},
      {"xdr:twoCellAnchor", parse_element_tree<Frame>},
  };

  if (auto constructor_it = parser_table.find(node.name());
      constructor_it != std::end(parser_table)) {
    return constructor_it->second(document, node);
  }

  return std::make_tuple(nullptr, pugi::xml_node());
}

} // namespace

} // namespace odr::internal::ooxml::spreadsheet

namespace odr::internal::ooxml {

spreadsheet::Element *spreadsheet::parse_tree(Document &document,
                                              pugi::xml_node node) {
  std::vector<std::unique_ptr<spreadsheet::Element>> store;
  auto [root_id, _] = parse_any_element_tree(document, node);
  return root_id;
}

} // namespace odr::internal::ooxml
