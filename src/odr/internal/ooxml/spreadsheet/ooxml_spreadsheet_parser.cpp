#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_parser.hpp>

#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_document.hpp>

#include "odr/internal/common/table_range.hpp"
#include <functional>
#include <unordered_map>

namespace odr::internal::ooxml::spreadsheet {

namespace {

template <typename element_t, typename... args_t>
std::tuple<element_t *, pugi::xml_node>
parse_element_tree(Document &document, pugi::xml_node node, args_t &&...args);
template <>
std::tuple<Sheet *, pugi::xml_node>
parse_element_tree<Sheet>(Document &document, pugi::xml_node node);

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
  for (auto child_node : node.child("sheets").children("sheet")) {
    const char *id = child_node.attribute("r:id").value();
    auto sheet_node = document.get_sheet_root(id);
    auto [sheet, _] = parse_element_tree<Sheet>(document, sheet_node);
    element->append_child_(sheet);
  }
}

template <typename element_t, typename... args_t>
std::tuple<element_t *, pugi::xml_node>
parse_element_tree(Document &document, pugi::xml_node node, args_t &&...args) {
  if (!node) {
    return std::make_tuple(nullptr, pugi::xml_node());
  }

  auto element_unique =
      std::make_unique<element_t>(node, std::forward<args_t>(args)...);
  auto element = element_unique.get();
  document.register_element_(std::move(element_unique));

  parse_element_children(document, element, node);

  return std::make_tuple(element, node.next_sibling());
}

template <>
std::tuple<Sheet *, pugi::xml_node>
parse_element_tree<Sheet>(Document &document, pugi::xml_node node) {
  if (!node) {
    return std::make_tuple(nullptr, pugi::xml_node());
  }

  auto element_unique = std::make_unique<Sheet>(node);
  auto element = element_unique.get();
  document.register_element_(std::move(element_unique));

  for (auto col_node : node.child("cols").children("col")) {
    std::uint32_t min = col_node.attribute("min").as_uint() - 1;
    std::uint32_t max = col_node.attribute("max").as_uint() - 1;
    element->init_column_(min, max, col_node);
  }

  for (auto row_node : node.child("sheetData").children("row")) {
    std::uint32_t row = row_node.attribute("r").as_uint() - 1;
    element->init_row_(row, row_node);

    for (auto cell_node : row_node.children("c")) {
      auto position = common::TablePosition(cell_node.attribute("r").value());
      element->init_cell_(position.column(), position.row(), cell_node);

      auto [cell, _] = parse_element_tree<SheetCell>(document, cell_node);
      element->init_cell_element_(position.column(), position.row(), cell);
    }
  }

  {
    std::string dimension_ref =
        node.child("dimension").attribute("ref").value();
    common::TablePosition position_to;
    if (dimension_ref.find(":") == std::string::npos) {
      position_to = common::TablePosition(dimension_ref);
    } else {
      position_to = common::TableRange(dimension_ref).to();
    }
    element->init_dimensions_(
        TableDimensions(position_to.row() + 1, position_to.column() + 1));
  }

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
std::tuple<Text *, pugi::xml_node>
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
