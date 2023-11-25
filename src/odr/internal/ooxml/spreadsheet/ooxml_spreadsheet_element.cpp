#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_element.hpp>

#include <odr/style.hpp>

#include <odr/internal/abstract/document.hpp>
#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/document_element.hpp>
#include <odr/internal/common/style.hpp>
#include <odr/internal/common/table_range.hpp>
#include <odr/internal/ooxml/ooxml_util.hpp>
#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_document.hpp>

#include <pugixml.hpp>

#include <functional>
#include <optional>
#include <unordered_map>
#include <utility>

namespace odr::internal::ooxml::spreadsheet {

Element::Element(pugi::xml_node node) : common::Element(node) {}

common::ResolvedStyle Element::partial_style(const abstract::Document *) const {
  return {};
}

common::ResolvedStyle
Element::intermediate_style(const abstract::Document *document) const {
  if (m_parent == nullptr) {
    return partial_style(document);
  }
  auto base = dynamic_cast<Element *>(m_parent)->intermediate_style(document);
  base.override(partial_style(document));
  return base;
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

ElementType Sheet::type(const abstract::Document *) const {
  return ElementType::sheet;
}

std::string Sheet::name(const abstract::Document *) const {
  return m_node.attribute("name").value();
}

TableDimensions Sheet::dimensions(const abstract::Document *document) const {
  if (auto dimension =
          sheet_node_(document).child("dimension").attribute("ref")) {
    try {
      auto range = common::TableRange(dimension.value());
      return {range.to().row() + 1, range.to().column() + 1};
    } catch (...) {
    }
  }
  return {};
}

TableDimensions Sheet::content(const abstract::Document *document,
                               std::optional<TableDimensions>) const {
  return dimensions(document); // TODO
}

abstract::Element *Sheet::column(const abstract::Document * /*document*/,
                                 std::uint32_t /*column*/) const {
  return nullptr; // TODO
}

abstract::Element *Sheet::row(const abstract::Document * /*document*/,
                              std::uint32_t /*row*/) const {
  return nullptr; // TODO
}

abstract::Element *Sheet::cell(const abstract::Document * /*document*/,
                               std::uint32_t /*column*/,
                               std::uint32_t /*row*/) const {
  return nullptr; // TODO
}

abstract::Element *
Sheet::first_shape(const abstract::Document * /*document*/) const {
  return nullptr; // TODO
}

TableStyle Sheet::style(const abstract::Document *document) const {
  return partial_style(document).table_style;
}
pugi::xml_node Sheet::sheet_node_(const abstract::Document *document) const {
  return sheet_(document, m_node.attribute("r:id").value());
}

pugi::xml_node Sheet::drawing_node_(const abstract::Document *document) const {
  return drawing_(document, m_node.attribute("r:id").value());
}

TableColumnStyle TableColumn::style(const abstract::Document *) const {
  TableColumnStyle result;
  if (auto width = m_node.attribute("width")) {
    result.width = Measure(width.as_float(), DynamicUnit("ch"));
  }
  return result;
}

[[nodiscard]] std::uint32_t TableColumn::min_() const {
  return m_node.attribute("min").as_uint() - 1;
}

[[nodiscard]] std::uint32_t TableColumn::max_() const {
  return m_node.attribute("min").as_uint() - 1;
}

TableRowStyle TableRow::style(const abstract::Document * /*document*/) const {
  TableRowStyle result;
  if (auto height = m_node.attribute("ht")) {
    result.height = Measure(height.as_float(), DynamicUnit("pt"));
  }
  return result;
}

abstract::Element *
TableCell::column(const abstract::Document * /*document*/) const {
  return nullptr; // TODO
}

abstract::Element *
TableCell::row(const abstract::Document * /*document*/) const {
  return nullptr; // TODO
}

bool TableCell::covered(const abstract::Document * /*document*/) const {
  return false; // TODO
}

ValueType TableCell::value_type(const abstract::Document * /*document*/) const {
  return ValueType::string;
}

common::ResolvedStyle
TableCell::partial_style(const abstract::Document *document) const {
  if (auto id = m_node.attribute("s")) {
    return style_registry_(document)->cell_style(id.as_uint());
  }
  return {};
}

TableDimensions TableCell::span(const abstract::Document * /*document*/) const {
  return {1, 1};
}

TableCellStyle TableCell::style(const abstract::Document *document) const {
  return partial_style(document).table_cell_style;
}

TextStyle Span::style(const abstract::Document *document) const {
  return intermediate_style(document).text_style;
}

Text::Text(pugi::xml_node node) : Text(node, node) {}

Text::Text(pugi::xml_node first, pugi::xml_node last)
    : Element(first), m_last{last} {}

std::string Text::content(const abstract::Document * /*document*/) const {
  std::string result;
  for (auto node = m_node; node != m_last.next_sibling();
       node = node.next_sibling()) {
    result += text_(node);
  }
  return result;
}

void Text::set_content(const abstract::Document * /*document*/,
                       const std::string & /*text*/) {
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

AnchorType Frame::anchor_type(const abstract::Document * /*document*/) const {
  return AnchorType::at_page;
}

std::optional<std::string>
Frame::x(const abstract::Document * /*document*/) const {
  if (auto x = read_emus_attribute(m_node.child("xdr:pic")
                                       .child("xdr:spPr")
                                       .child("a:xfrm")
                                       .child("a:off")
                                       .attribute("x"))) {
    return x->to_string();
  }
  return {};
}

std::optional<std::string>
Frame::y(const abstract::Document * /*document*/) const {
  if (auto y = read_emus_attribute(m_node.child("xdr:pic")
                                       .child("xdr:spPr")
                                       .child("a:xfrm")
                                       .child("a:off")
                                       .attribute("y"))) {
    return y->to_string();
  }
  return {};
}

std::optional<std::string>
Frame::width(const abstract::Document * /*document*/) const {
  if (auto width = read_emus_attribute(m_node.child("xdr:pic")
                                           .child("xdr:spPr")
                                           .child("a:xfrm")
                                           .child("a:ext")
                                           .attribute("cx"))) {
    return width->to_string();
  }
  return {};
}

std::optional<std::string>
Frame::height(const abstract::Document * /*document*/) const {
  if (auto height = read_emus_attribute(m_node.child("xdr:pic")
                                            .child("xdr:spPr")
                                            .child("a:xfrm")
                                            .child("a:ext")
                                            .attribute("cy"))) {
    return height->to_string();
  }
  return {};
}

std::optional<std::string>
Frame::z_index(const abstract::Document * /*document*/) const {
  return {};
}

GraphicStyle Frame::style(const abstract::Document * /*document*/) const {
  return {};
}

bool ImageElement::internal(const abstract::Document *document) const {
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
  if (!doc || !internal(document)) {
    return {};
  }
  return File(doc->files()->open(href(document)));
}

std::string ImageElement::href(const abstract::Document * /*document*/) const {
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
