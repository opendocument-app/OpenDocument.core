#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_element.hpp>

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/table_range.hpp>
#include <odr/internal/ooxml/ooxml_util.hpp>
#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_document.hpp>
#include <odr/internal/util/map_util.hpp>

#include <functional>
#include <optional>

#include <pugixml.hpp>

namespace odr::internal::ooxml::spreadsheet {

Element::Element(pugi::xml_node node) : m_node{node} {
  if (!node) {
    // TODO log error
    throw std::runtime_error("node not set");
  }
}

common::ResolvedStyle Element::partial_style(const abstract::Document *) const {
  return {};
}

common::ResolvedStyle
Element::intermediate_style(const abstract::Document *document) const {
  abstract::Element *parent = this->parent(document);
  if (parent == nullptr) {
    return partial_style(document);
  }
  auto base = dynamic_cast<Element *>(parent)->intermediate_style(document);
  base.override(partial_style(document));
  return base;
}

bool Element::is_editable(const abstract::Document *document) const {
  if (m_parent == nullptr) {
    return document_(document)->is_editable();
  }
  return m_parent->is_editable(document);
}

const Document *Element::document_(const abstract::Document *document) {
  return dynamic_cast<const Document *>(document);
}

const StyleRegistry *
Element::style_registry_(const abstract::Document *document) {
  return &document_(document)->m_style_registry;
}

pugi::xml_node Element::sheet_(const abstract::Document *document,
                               const std::string &id) {
  return document_(document)->m_sheets.at(id).sheet_xml.document_element();
}

pugi::xml_node Element::drawing_(const abstract::Document *document,
                                 const std::string &id) {
  return document_(document)->m_sheets.at(id).drawing_xml.document_element();
}

const std::vector<pugi::xml_node> &
Element::shared_strings_(const abstract::Document *document) {
  return document_(document)->m_shared_strings;
}

void SheetIndex::init_column(std::uint32_t /*min*/, std::uint32_t max,
                             pugi::xml_node element) {
  columns[max] = element;
}

void SheetIndex::init_row(std::uint32_t row, pugi::xml_node element) {
  rows[row].row = element;
}

void SheetIndex::init_cell(std::uint32_t column, std::uint32_t row,
                           pugi::xml_node element) {
  rows[row].cells[column] = element;
}

pugi::xml_node SheetIndex::column(std::uint32_t column) const {
  if (auto it = util::map::lookup_greater_than(columns, column);
      it != std::end(columns)) {
    return it->second;
  }
  return {};
}

pugi::xml_node SheetIndex::row(std::uint32_t row) const {
  if (auto it = util::map::lookup_greater_than(rows, row);
      it != std::end(rows)) {
    return it->second.row;
  }
  return {};
}

pugi::xml_node SheetIndex::cell(std::uint32_t column, std::uint32_t row) const {
  if (auto row_it = util::map::lookup_greater_than(rows, row);
      row_it != std::end(rows)) {
    const auto &cells = row_it->second.cells;
    if (auto cell_it = util::map::lookup_greater_than(cells, column);
        cell_it != std::end(cells)) {
      return cell_it->second;
    }
  }
  return {};
}

std::string Sheet::name(const abstract::Document *) const {
  return m_node.attribute("name").value();
}

TableDimensions
Sheet::dimensions(const abstract::Document * /*document*/) const {
  return m_index.dimensions;
}

TableDimensions Sheet::content(const abstract::Document *document,
                               std::optional<TableDimensions>) const {
  return dimensions(document); // TODO
}

abstract::SheetCell *Sheet::cell(const abstract::Document *,
                                 std::uint32_t column,
                                 std::uint32_t row) const {
  if (auto cell_it = m_cells.find({column, row});
      cell_it != std::end(m_cells)) {
    return cell_it->second;
  }
  return nullptr;
}

abstract::Element *Sheet::first_shape(const abstract::Document *) const {
  return m_first_shape;
}

TableStyle Sheet::style(const abstract::Document *) const {
  return TableStyle(); // TODO
}

TableColumnStyle Sheet::column_style(const abstract::Document *,
                                     std::uint32_t column) const {
  TableColumnStyle result;
  pugi::xml_node column_node = m_index.column(column);
  if (auto width = column_node.attribute("width")) {
    result.width = Measure(width.as_float(), DynamicUnit("ch"));
  }
  return result;
}

TableRowStyle Sheet::row_style(const abstract::Document *,
                               std::uint32_t row) const {
  TableRowStyle result;
  pugi::xml_node row_node = m_index.row(row);
  if (auto height = row_node.attribute("ht")) {
    result.height = Measure(height.as_float(), DynamicUnit("pt"));
  }
  return result;
}

TableCellStyle Sheet::cell_style(const abstract::Document *,
                                 std::uint32_t /*column*/,
                                 std::uint32_t /*row*/) const {
  return TableCellStyle(); // TODO
}

void Sheet::init_column_(std::uint32_t min, std::uint32_t max,
                         pugi::xml_node element) {
  m_index.init_column(min, max, element);
}

void Sheet::init_row_(std::uint32_t row, pugi::xml_node element) {
  m_index.init_row(row, element);
}

void Sheet::init_cell_(std::uint32_t column, std::uint32_t row,
                       pugi::xml_node element) {
  m_index.init_cell(column, row, element);
}

void Sheet::init_cell_element_(std::uint32_t column, std::uint32_t row,
                               SheetCell *element) {
  m_cells[{column, row}] = element;
  element->m_parent = this;
}

void Sheet::init_dimensions_(TableDimensions dimensions) {
  m_index.dimensions = dimensions;
}

pugi::xml_node Sheet::sheet_node_(const abstract::Document *document) const {
  return sheet_(document, m_node.attribute("r:id").value());
}

pugi::xml_node Sheet::drawing_node_(const abstract::Document *document) const {
  return drawing_(document, m_node.attribute("r:id").value());
}

bool SheetCell::is_covered(const abstract::Document *) const {
  return false; // TODO
}

ValueType SheetCell::value_type(const abstract::Document *) const {
  return ValueType::string;
}

common::ResolvedStyle
SheetCell::partial_style(const abstract::Document *document) const {
  if (auto style_id = m_node.attribute("s")) {
    return style_registry_(document)->cell_style(style_id.as_uint());
  }
  return {};
}

TableDimensions SheetCell::span(const abstract::Document *) const {
  return {1, 1};
}

TableCellStyle SheetCell::style(const abstract::Document *document) const {
  return partial_style(document).table_cell_style;
}

TextStyle Span::style(const abstract::Document *document) const {
  return intermediate_style(document).text_style;
}

Text::Text(pugi::xml_node node) : Text(node, node) {}

Text::Text(pugi::xml_node first, pugi::xml_node last)
    : Element(first), m_last{last} {}

std::string Text::content(const abstract::Document *) const {
  std::string result;
  for (auto node = m_node; node != m_last.next_sibling();
       node = node.next_sibling()) {
    result += text_(node);
  }
  return result;
}

void Text::set_content(const abstract::Document *, const std::string &) {
  // TODO
}

TextStyle Text::style(const abstract::Document *document) const {
  return intermediate_style(document).text_style;
}

std::string Text::text_(const pugi::xml_node node) {
  std::string name = node.name();

  if (name == "t" || name == "v") {
    return node.text().get();
  }

  return "";
}

AnchorType Frame::anchor_type(const abstract::Document *) const {
  return AnchorType::at_page;
}

std::optional<std::string> Frame::x(const abstract::Document *) const {
  if (auto x = read_emus_attribute(m_node.child("xdr:pic")
                                       .child("xdr:spPr")
                                       .child("a:xfrm")
                                       .child("a:off")
                                       .attribute("x"))) {
    return x->to_string();
  }
  return {};
}

std::optional<std::string> Frame::y(const abstract::Document *) const {
  if (auto y = read_emus_attribute(m_node.child("xdr:pic")
                                       .child("xdr:spPr")
                                       .child("a:xfrm")
                                       .child("a:off")
                                       .attribute("y"))) {
    return y->to_string();
  }
  return {};
}

std::optional<std::string> Frame::width(const abstract::Document *) const {
  if (auto width = read_emus_attribute(m_node.child("xdr:pic")
                                           .child("xdr:spPr")
                                           .child("a:xfrm")
                                           .child("a:ext")
                                           .attribute("cx"))) {
    return width->to_string();
  }
  return {};
}

std::optional<std::string> Frame::height(const abstract::Document *) const {
  if (auto height = read_emus_attribute(m_node.child("xdr:pic")
                                            .child("xdr:spPr")
                                            .child("a:xfrm")
                                            .child("a:ext")
                                            .attribute("cy"))) {
    return height->to_string();
  }
  return {};
}

std::optional<std::string> Frame::z_index(const abstract::Document *) const {
  return {};
}

GraphicStyle Frame::style(const abstract::Document *) const { return {}; }

bool ImageElement::is_internal(const abstract::Document *document) const {
  auto doc = document_(document);
  if (!doc || !doc->files()) {
    return false;
  }
  try {
    return doc->files()->is_file(href(document));
  } catch (...) {
  }
  return false;
}

std::optional<odr::File>
ImageElement::file(const abstract::Document *document) const {
  auto doc = document_(document);
  if (!doc || !is_internal(document)) {
    return {};
  }
  return File(doc->files()->open(href(document)));
}

std::string ImageElement::href(const abstract::Document *) const {
  if (auto ref = m_node.attribute("r:embed")) {
    /* TODO
    auto relations = document_relations_(document);
    if (auto rel = relations.find(ref.value()); rel != std::end(relations)) {
      return common::Path("word").join(rel->second).string();
    }*/
  }
  return ""; // TODO
}

} // namespace odr::internal::ooxml::spreadsheet
