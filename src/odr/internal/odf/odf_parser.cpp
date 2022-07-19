#include <functional>
#include <odr/internal/odf/odf_element.hpp>
#include <odr/internal/odf/odf_parser.hpp>
#include <unordered_map>

namespace odr::internal::odf {

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

  using List = DefaultElement<ElementType::list>;
  using Group = DefaultElement<ElementType::group>;
  using PageBreak = DefaultElement<ElementType::page_break>;

  static std::unordered_map<std::string, Parser> parser_table{
      {"office:text", default_parse_element_tree<TextDocumentRoot>},
      {"office:presentation", default_parse_element_tree<PresentationRoot>},
      {"office:spreadsheet", default_parse_element_tree<SpreadsheetRoot>},
      {"office:drawing", default_parse_element_tree<DrawingRoot>},
      {"text:p", default_parse_element_tree<Paragraph>},
      {"text:h", default_parse_element_tree<Paragraph>},
      {"text:span", default_parse_element_tree<Span>},
      {"text:s", default_parse_element_tree<Text>},
      {"text:tab", default_parse_element_tree<Text>},
      {"text:line-break", default_parse_element_tree<LineBreak>},
      {"text:a", default_parse_element_tree<Link>},
      {"text:bookmark", default_parse_element_tree<Bookmark>},
      {"text:bookmark-start", default_parse_element_tree<Bookmark>},
      {"text:list", default_parse_element_tree<List>},
      {"text:list-header", default_parse_element_tree<ListItem>},
      {"text:list-item", default_parse_element_tree<ListItem>},
      {"text:index-title", default_parse_element_tree<Group>},
      {"text:table-of-content", default_parse_element_tree<Group>},
      {"text:illustration-index", default_parse_element_tree<Group>},
      {"text:index-body", default_parse_element_tree<Group>},
      {"text:soft-page-break", default_parse_element_tree<PageBreak>},
      {"text:date", default_parse_element_tree<Group>},
      {"text:time", default_parse_element_tree<Group>},
      {"text:section", default_parse_element_tree<Group>},
      //{"text:page-number", default_parse_element_tree<Group>},
      //{"text:page-continuation", default_parse_element_tree<Group>},
      {"table:table", default_parse_element_tree<TableElement>},
      {"table:table-column", default_parse_element_tree<TableColumn>},
      {"table:table-row", default_parse_element_tree<TableRow>},
      {"table:table-cell", default_parse_element_tree<TableCell>},
      {"table:covered-table-cell", default_parse_element_tree<TableCell>},
      {"draw:frame", default_parse_element_tree<Frame>},
      {"draw:image", default_parse_element_tree<ImageElement>},
      {"draw:rect", default_parse_element_tree<Rect>},
      {"draw:line", default_parse_element_tree<Line>},
      {"draw:circle", default_parse_element_tree<Circle>},
      {"draw:custom-shape", default_parse_element_tree<CustomShape>},
      {"draw:text-box", default_parse_element_tree<Group>},
      {"draw:g", default_parse_element_tree<Group>},
      {"draw:a", default_parse_element_tree<Link>},
  };

  if (node.type() == pugi::xml_node_type::node_pcdata) {
    return default_parse_element_tree<Text>(node, store);
  }

  if (auto constructor_it = parser_table.find(node.name());
      constructor_it != std::end(parser_table)) {
    return constructor_it->second(node, store);
  }

  return std::make_tuple(nullptr, pugi::xml_node());
}

} // namespace

} // namespace odr::internal::odf

namespace odr::internal {

std::tuple<abstract::Element *, std::vector<std::unique_ptr<abstract::Element>>>
odf::parse_tree(pugi::xml_node node) {
  std::vector<std::unique_ptr<abstract::Element>> store;
  auto [root, _] = parse_element_tree(node, store);
  return std::make_tuple(root, store);
}

} // namespace odr::internal
