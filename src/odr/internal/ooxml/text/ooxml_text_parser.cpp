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

bool is_text_node(pugi::xml_node node) {
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

bool is_list_item(pugi::xml_node node) {
  return node.child("w:pPr").child("w:numPr");
}

std::int32_t list_level(pugi::xml_node node) {
  return node.child("w:pPr")
      .child("w:numPr")
      .child("w:ilvl")
      .attribute("w:val")
      .as_int(0);
}

template <>
std::tuple<Element *, pugi::xml_node>
parse_element_tree<List>(pugi::xml_node first,
                         std::vector<std::unique_ptr<Element>> &store) {
  if (!first) {
    return std::make_tuple(nullptr, pugi::xml_node());
  }

  auto list_unique = std::make_unique<List>(first);
  auto list = list_unique.get();
  store.push_back(std::move(list_unique));

  pugi::xml_node node = first;
  for (; is_list_item(node); node = node.next_sibling()) {
    auto base = list;
    auto level = list_level(node);

    for (std::int32_t i = 0; i < level; ++i) {
      /* TODO fix lists
      auto list_item_unique = std::make_unique<ListItem>(node);
      auto list_item = list_item_unique.get();
      store.push_back(std::move(list_item_unique));

      base->init_append_child(list_item);
       */

      auto nested_list_unique = std::make_unique<List>(node);
      auto nested_list = nested_list_unique.get();
      store.push_back(std::move(nested_list_unique));

      // list_item->init_append_child(nested_list);

      base->init_append_child(nested_list);
      base = nested_list;
    }

    auto list_item_unique = std::make_unique<ListItem>(node);
    auto list_item = list_item_unique.get();
    store.push_back(std::move(list_item_unique));

    base->init_append_child(list_item);

    auto [element, _] = parse_element_tree<Paragraph>(node, store);
    list_item->init_append_child(element);
  }

  return std::make_tuple(list, node);
}

template <>
std::tuple<Element *, pugi::xml_node>
parse_element_tree<TableRow>(pugi::xml_node node,
                             std::vector<std::unique_ptr<Element>> &store) {
  if (!node) {
    return std::make_tuple(nullptr, pugi::xml_node());
  }

  auto table_row_unique = std::make_unique<TableRow>(node);
  auto table_row = table_row_unique.get();
  store.push_back(std::move(table_row_unique));

  for (auto cell_node : node.children("w:tc")) {
    auto [cell, _] = parse_element_tree<TableCell>(cell_node, store);
    table_row->init_append_child(cell);
  }

  return std::make_tuple(table_row, node.next_sibling());
}

template <>
std::tuple<Element *, pugi::xml_node>
parse_element_tree<Table>(pugi::xml_node node,
                          std::vector<std::unique_ptr<Element>> &store) {
  if (!node) {
    return std::make_tuple(nullptr, pugi::xml_node());
  }

  auto table_unique = std::make_unique<Table>(node);
  auto table = table_unique.get();
  store.push_back(std::move(table_unique));

  for (auto column_node : node.child("w:tblGrid").children("w:gridCol")) {
    auto [column, _] = parse_element_tree<TableColumn>(column_node, store);
    table->init_append_column(column);
  }

  for (auto row_node : node.children("w:tr")) {
    auto [row, _] = parse_element_tree<TableRow>(row_node, store);
    table->init_append_row(row);
  }

  return std::make_tuple(table, node.next_sibling());
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
      {"w:tbl", parse_element_tree<Table>},
      {"w:gridCol", parse_element_tree<TableColumn>},
      {"w:tr", parse_element_tree<TableRow>},
      {"w:tc", parse_element_tree<TableCell>},
      {"w:sdt", parse_element_tree<Group>},
      {"w:sdtContent", parse_element_tree<Group>},
      {"w:drawing", parse_element_tree<Frame>},
      {"wp:anchor", parse_element_tree<Group>},
      {"wp:inline", parse_element_tree<Group>},
      {"a:graphic", parse_element_tree<Group>},
      {"a:graphicData", parse_element_tree<Image>},
  };

  if (is_list_item(node)) {
    return parse_element_tree<List>(node, store);
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
