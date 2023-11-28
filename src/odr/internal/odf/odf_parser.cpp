#include <odr/internal/odf/odf_parser.hpp>

#include <odr/internal/odf/odf_element.hpp>
#include <odr/internal/odf/odf_spreadsheet.hpp>

#include <functional>
#include <unordered_map>

namespace odr::internal::odf {

namespace {

bool is_text_node(const pugi::xml_node node) {
  if (!node) {
    return false;
  }
  if (node.type() == pugi::node_pcdata) {
    return true;
  }

  std::string name = node.name();

  if (name == "text:s") {
    return true;
  }
  if (name == "text:tab") {
    return true;
  }

  return false;
}

} // namespace

} // namespace odr::internal::odf

namespace odr::internal {

odf::Element *odf::parse_tree(pugi::xml_node node,
                              std::vector<std::unique_ptr<Element>> &store) {
  auto [root, _] = parse_any_element_tree(node, store);
  return root;
}

std::tuple<odf::Element *, std::vector<std::unique_ptr<odf::Element>>>
odf::parse_tree(pugi::xml_node node) {
  std::vector<std::unique_ptr<odf::Element>> store;
  auto root = parse_tree(node, store);
  return std::make_tuple(root, std::move(store));
}

void odf::parse_element_children(Element *element, pugi::xml_node node,
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

void odf::parse_element_children(PresentationRoot *element, pugi::xml_node node,
                                 std::vector<std::unique_ptr<Element>> &store) {
  for (auto child_node : node.children("draw:page")) {
    auto [child, _] = parse_element_tree<Slide>(child_node, store);
    element->init_append_child(child);
  }
}

void odf::parse_element_children(SpreadsheetRoot *element, pugi::xml_node node,
                                 std::vector<std::unique_ptr<Element>> &store) {
  for (auto child_node : node.children("table:table")) {
    auto [child, _] = parse_element_tree<Sheet>(child_node, store);
    element->init_append_child(child);
  }
}

void odf::parse_element_children(DrawingRoot *element, pugi::xml_node node,
                                 std::vector<std::unique_ptr<Element>> &store) {
  for (auto child_node : node.children("draw:page")) {
    auto [child, _] = parse_element_tree<Page>(child_node, store);
    element->init_append_child(child);
  }
}

template <>
std::tuple<odf::Element *, pugi::xml_node> odf::parse_element_tree<odf::Text>(
    pugi::xml_node first, std::vector<std::unique_ptr<Element>> &store) {
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

template <>
std::tuple<odf::Element *, pugi::xml_node> odf::parse_element_tree<odf::Table>(
    pugi::xml_node node, std::vector<std::unique_ptr<Element>> &store) {
  if (!node) {
    return std::make_tuple(nullptr, pugi::xml_node());
  }

  auto table_unique = std::make_unique<Table>(node);
  auto table = table_unique.get();
  store.push_back(std::move(table_unique));

  // TODO inflate table first?

  for (auto column_node : node.children("table:table-column")) {
    for (std::uint32_t i = 0;
         i < column_node.attribute("table:number-columns-repeated").as_uint(1);
         ++i) {
      // TODO mark as repeated
      auto [column, _] = parse_element_tree<TableColumn>(column_node, store);
      table->init_append_column(column);
    }
  }

  for (auto row_node : node.children("table:table-row")) {
    // TODO log warning if repeated
    auto [row, _] = parse_element_tree<TableRow>(row_node, store);
    table->init_append_row(row);
  }

  return std::make_tuple(table, node.next_sibling());
}

template <>
std::tuple<odf::Element *, pugi::xml_node>
odf::parse_element_tree<odf::TableRow>(
    pugi::xml_node node, std::vector<std::unique_ptr<Element>> &store) {
  if (!node) {
    return std::make_tuple(nullptr, pugi::xml_node());
  }

  auto table_row_unique = std::make_unique<TableRow>(node);
  auto table_row = table_row_unique.get();
  store.push_back(std::move(table_row_unique));

  for (auto cell_node : node.children()) {
    // TODO log warning if repeated
    auto [cell, _] = parse_any_element_tree(cell_node, store);
    table_row->init_append_child(cell);
  }

  return std::make_tuple(table_row, node.next_sibling());
}

std::tuple<odf::Element *, pugi::xml_node>
odf::parse_any_element_tree(pugi::xml_node node,
                            std::vector<std::unique_ptr<Element>> &store) {
  using Parser = std::function<std::tuple<Element *, pugi::xml_node>(
      pugi::xml_node node, std::vector<std::unique_ptr<Element>> & store)>;

  using List = DefaultElement<ElementType::list>;
  using Group = DefaultElement<ElementType::group>;
  using PageBreak = DefaultElement<ElementType::page_break>;

  static std::unordered_map<std::string, Parser> parser_table{
      {"office:text", parse_element_tree<TextRoot>},
      {"office:presentation", parse_element_tree<PresentationRoot>},
      {"office:spreadsheet", parse_element_tree<SpreadsheetRoot>},
      {"office:drawing", parse_element_tree<DrawingRoot>},
      {"text:p", parse_element_tree<Paragraph>},
      {"text:h", parse_element_tree<Paragraph>},
      {"text:span", parse_element_tree<Span>},
      {"text:s", parse_element_tree<Text>},
      {"text:tab", parse_element_tree<Text>},
      {"text:line-break", parse_element_tree<LineBreak>},
      {"text:a", parse_element_tree<Link>},
      {"text:bookmark", parse_element_tree<Bookmark>},
      {"text:bookmark-start", parse_element_tree<Bookmark>},
      {"text:list", parse_element_tree<List>},
      {"text:list-header", parse_element_tree<ListItem>},
      {"text:list-item", parse_element_tree<ListItem>},
      {"text:index-title", parse_element_tree<Group>},
      {"text:table-of-content", parse_element_tree<Group>},
      {"text:illustration-index", parse_element_tree<Group>},
      {"text:index-body", parse_element_tree<Group>},
      {"text:soft-page-break", parse_element_tree<PageBreak>},
      {"text:date", parse_element_tree<Group>},
      {"text:time", parse_element_tree<Group>},
      {"text:section", parse_element_tree<Group>},
      //{"text:page-number", parse_element_tree<Group>},
      //{"text:page-continuation", parse_element_tree<Group>},
      {"table:table", parse_element_tree<Table>},
      {"table:table-column", parse_element_tree<TableColumn>},
      {"table:table-row", parse_element_tree<TableRow>},
      {"table:table-cell", parse_element_tree<TableCell>},
      {"table:covered-table-cell", parse_element_tree<TableCell>},
      {"draw:frame", parse_element_tree<Frame>},
      {"draw:image", parse_element_tree<Image>},
      {"draw:rect", parse_element_tree<Rect>},
      {"draw:line", parse_element_tree<Line>},
      {"draw:circle", parse_element_tree<Circle>},
      {"draw:custom-shape", parse_element_tree<CustomShape>},
      {"draw:text-box", parse_element_tree<Group>},
      {"draw:g", parse_element_tree<Group>},
      {"draw:a", parse_element_tree<Link>},
      {"style:master-page", parse_element_tree<MasterPage>}};

  if (node.type() == pugi::xml_node_type::node_pcdata) {
    return parse_element_tree<Text>(node, store);
  }

  if (auto constructor_it = parser_table.find(node.name());
      constructor_it != std::end(parser_table)) {
    return constructor_it->second(node, store);
  }

  return std::make_tuple(nullptr, pugi::xml_node());
}

} // namespace odr::internal