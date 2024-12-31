#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_parser.hpp>

#include <odr/internal/common/table_range.hpp>
#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_document.hpp>

#include <unordered_map>

namespace odr::internal::ooxml::spreadsheet {

namespace {

template <typename element_t, typename... args_t>
std::tuple<element_t *, pugi::xml_node>
parse_element_tree(Document &document, pugi::xml_node node,
                   const common::Path &document_path,
                   const Relations &document_relations);
template <>
std::tuple<Sheet *, pugi::xml_node>
parse_element_tree<Sheet>(Document &document, pugi::xml_node node,
                          const common::Path &document_path,
                          const Relations &document_relations);

std::tuple<Element *, pugi::xml_node>
parse_any_element_tree(Document &document, pugi::xml_node node,
                       const common::Path &document_path,
                       const Relations &document_relations);

void parse_element_children(Document &document, Element *element,
                            pugi::xml_node node,
                            const common::Path &document_path,
                            const Relations &document_relations) {
  for (pugi::xml_node child_node = node.first_child(); child_node;) {
    auto [child, next_sibling] = parse_any_element_tree(
        document, child_node, document_path, document_relations);
    if (child == nullptr) {
      child_node = child_node.next_sibling();
    } else {
      element->append_child_(child);
      child_node = next_sibling;
    }
  }
}

void parse_element_children(Document &document, Root *element,
                            pugi::xml_node node,
                            const common::Path &document_path,
                            const Relations &document_relations) {
  for (pugi::xml_node child_node : node.child("sheets").children("sheet")) {
    const char *id = child_node.attribute("r:id").value();
    common::Path sheet_path =
        document_path.parent().join(common::Path(document_relations.at(id)));
    auto [sheet_xml, sheet_relations] = document.get_xml(sheet_path);
    auto [sheet, _] = parse_element_tree<Sheet>(
        document, sheet_xml.document_element(), sheet_path, sheet_relations);
    element->append_child_(sheet);
  }
}

void parse_element_children(Document &document, SheetCell *element,
                            pugi::xml_node node,
                            const common::Path &document_path,
                            const Relations &document_relations) {
  if (pugi::xml_attribute type_attr = node.attribute("t");
      type_attr.value() == std::string("s")) {
    pugi::xml_node v_node = node.child("v");
    std::size_t ref = v_node.first_child().text().as_ullong();
    pugi::xml_node shared_node = document.get_shared_string(ref);
    parse_element_children(document, dynamic_cast<Element *>(element),
                           shared_node, document_path, document_relations);
    return;
  }

  parse_element_children(document, dynamic_cast<Element *>(element), node,
                         document_path, document_relations);
}

void parse_element_children(Document &document, Frame *element,
                            pugi::xml_node node,
                            const common::Path &document_path,
                            const Relations &document_relations) {
  if (pugi::xml_node image_node =
          node.child("xdr:pic").child("xdr:blipFill").child("a:blip")) {
    auto [image, _] = parse_element_tree<ImageElement>(
        document, image_node, document_path, document_relations);
    element->append_child_(image);
  }
}

template <typename element_t, typename... args_t>
std::tuple<element_t *, pugi::xml_node>
parse_element_tree(Document &document, pugi::xml_node node,
                   const common::Path &document_path,
                   const Relations &document_relations) {
  if (!node) {
    return std::make_tuple(nullptr, pugi::xml_node());
  }

  auto element_unique =
      std::make_unique<element_t>(node, document_path, document_relations);
  auto element = element_unique.get();
  document.register_element_(std::move(element_unique));

  parse_element_children(document, element, node, document_path,
                         document_relations);

  return std::make_tuple(element, node.next_sibling());
}

template <>
std::tuple<Sheet *, pugi::xml_node>
parse_element_tree(Document &document, pugi::xml_node node,
                   const common::Path &document_path,
                   const Relations &document_relations) {
  if (!node) {
    return std::make_tuple(nullptr, pugi::xml_node());
  }

  auto element_unique =
      std::make_unique<Sheet>(node, document_path, document_relations);
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

      auto [cell, _] = parse_element_tree<SheetCell>(
          document, cell_node, document_path, document_relations);
      element->init_cell_element_(position.column(), position.row(), cell);
    }
  }

  {
    std::string dimension_ref =
        node.child("dimension").attribute("ref").value();
    common::TablePosition position_to;
    if (dimension_ref.find(':') == std::string::npos) {
      position_to = common::TablePosition(dimension_ref);
    } else {
      position_to = common::TableRange(dimension_ref).to();
    }
    element->init_dimensions_(
        TableDimensions(position_to.row() + 1, position_to.column() + 1));
  }

  if (pugi::xml_node drawing_node = node.child("drawing")) {
    const char *id = drawing_node.attribute("r:id").value();
    common::Path drawing_path =
        document_path.parent().join(common::Path(document_relations.at(id)));
    auto [drawing_xml, drawing_relations] = document.get_xml(drawing_path);

    for (pugi::xml_node shape_node :
         drawing_xml.document_element().children()) {
      auto [shape, _] = parse_any_element_tree(document, shape_node,
                                               drawing_path, drawing_relations);
      if (shape != nullptr) {
        element->append_shape_(shape);
      }
    }
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
parse_element_tree<Text>(Document &document, pugi::xml_node first,
                         const common::Path &document_path,
                         const Relations &document_relations) {
  if (!first) {
    return std::make_tuple(nullptr, pugi::xml_node());
  }

  pugi::xml_node last = first;
  for (; is_text_node(last.next_sibling()); last = last.next_sibling()) {
  }

  auto element_unique =
      std::make_unique<Text>(first, last, document_path, document_relations);
  auto element = element_unique.get();
  document.register_element_(std::move(element_unique));

  return std::make_tuple(element, last.next_sibling());
}

std::tuple<Element *, pugi::xml_node>
parse_any_element_tree(Document &document, pugi::xml_node node,
                       const common::Path &document_path,
                       const Relations &document_relations) {
  using Parser = std::function<std::tuple<Element *, pugi::xml_node>(
      Document & document, pugi::xml_node node,
      const common::Path &document_path, const Relations &document_relations)>;

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
    return constructor_it->second(document, node, document_path,
                                  document_relations);
  }

  return std::make_tuple(nullptr, pugi::xml_node());
}

} // namespace

} // namespace odr::internal::ooxml::spreadsheet

namespace odr::internal::ooxml {

spreadsheet::Element *
spreadsheet::parse_tree(Document &document, pugi::xml_node node,
                        const common::Path &document_path,
                        const Relations &document_relations) {
  auto [root_id, _] =
      parse_any_element_tree(document, node, document_path, document_relations);
  return root_id;
}

} // namespace odr::internal::ooxml
