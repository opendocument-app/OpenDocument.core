#include <odr/internal/ooxml/presentation/ooxml_presentation_parser.hpp>

#include <odr/internal/ooxml/presentation/ooxml_presentation_document.hpp>

#include <functional>
#include <unordered_map>

namespace odr::internal::ooxml::presentation {

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

void parse_element_children(Document &document, Root *element,
                            pugi::xml_node node) {
  for (auto child_node : node.child("p:sldIdLst").children("p:sldId")) {
    const char *id = child_node.attribute("r:id").value();
    auto slide_node = document.m_slides_xml.at(id).document_element();
    auto [slide, _] = parse_element_tree<Slide>(document, slide_node);
    element->append_child_(slide);
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

  using Group = DefaultElement<ElementType::group>;

  static std::unordered_map<std::string, Parser> parser_table{
      {"p:presentation", parse_element_tree<Root>},
      {"p:sld", parse_element_tree<Slide>},
      {"p:sp", parse_element_tree<Frame>},
      {"p:txBody", parse_element_tree<Group>},
      {"a:t", parse_element_tree<Text>},
      {"a:p", parse_element_tree<Paragraph>},
      {"a:r", parse_element_tree<Span>},
      {"a:tbl", parse_element_tree<TableElement>},
      {"a:gridCol", parse_element_tree<TableColumn>},
      {"a:tr", parse_element_tree<TableRow>},
      {"a:tc", parse_element_tree<TableCell>},
  };

  if (auto constructor_it = parser_table.find(node.name());
      constructor_it != std::end(parser_table)) {
    return constructor_it->second(document, node);
  }

  return std::make_tuple(nullptr, pugi::xml_node());
}

} // namespace

} // namespace odr::internal::ooxml::presentation

namespace odr::internal::ooxml {

presentation::Element *
presentation::parse_tree(presentation::Document &document,
                         pugi::xml_node node) {
  auto [root, _] = parse_any_element_tree(document, node);
  return root;
}

} // namespace odr::internal::ooxml
