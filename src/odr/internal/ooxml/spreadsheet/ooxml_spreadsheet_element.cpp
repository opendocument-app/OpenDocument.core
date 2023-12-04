#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_element.hpp>

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/table_range.hpp>
#include <odr/internal/ooxml/ooxml_util.hpp>
#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_document.hpp>

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

abstract::SheetColumn *Sheet::column(const abstract::Document *,
                                     std::uint32_t /*column*/) const {
  return nullptr; // TODO
}

abstract::SheetRow *Sheet::row(const abstract::Document *,
                               std::uint32_t /*row*/) const {
  return nullptr; // TODO
}

abstract::SheetCell *Sheet::cell(const abstract::Document *,
                                 std::uint32_t /*column*/,
                                 std::uint32_t /*row*/) const {
  return nullptr; // TODO
}

abstract::Element *Sheet::first_shape(const abstract::Document *) const {
  return nullptr; // TODO
}

pugi::xml_node Sheet::sheet_node_(const abstract::Document *document) const {
  return sheet_(document, m_node.attribute("r:id").value());
}

pugi::xml_node Sheet::drawing_node_(const abstract::Document *document) const {
  return drawing_(document, m_node.attribute("r:id").value());
}

TableColumnStyle SheetColumn::style(const abstract::Document *,
                                    const abstract::Sheet *,
                                    std::uint32_t /*column*/) const {
  TableColumnStyle result;
  if (auto width = m_node.attribute("width")) {
    result.width = Measure(width.as_float(), DynamicUnit("ch"));
  }
  return result;
}

[[nodiscard]] std::uint32_t
SheetColumn::min_(const abstract::Document *) const {
  return m_node.attribute("min").as_uint() - 1;
}

[[nodiscard]] std::uint32_t
SheetColumn::max_(const abstract::Document *) const {
  return m_node.attribute("max").as_uint() - 1;
}

TableRowStyle SheetRow::style(const abstract::Document *,
                              const abstract::Sheet *,
                              std::uint32_t /*row*/) const {
  TableRowStyle result;
  if (auto height = m_node.attribute("ht")) {
    result.height = Measure(height.as_float(), DynamicUnit("pt"));
  }
  return result;
}

bool SheetCell::is_covered(const abstract::Document *, const abstract::Sheet *,
                           std::uint32_t /*column*/,
                           std::uint32_t /*row*/) const {
  return false; // TODO
}

ValueType SheetCell::value_type(const abstract::Document *,
                                const abstract::Sheet *,
                                std::uint32_t /*column*/,
                                std::uint32_t /*row*/) const {
  return ValueType::string;
}

common::ResolvedStyle
SheetCell::partial_style(const abstract::Document *document) const {
  if (auto style_id = m_node.attribute("s")) {
    return style_registry_(document)->cell_style(style_id.as_uint());
  }
  return {};
}

TableDimensions SheetCell::span(const abstract::Document *document,
                                const abstract::Sheet *,
                                std::uint32_t /*column*/,
                                std::uint32_t /*row*/) const {
  return {1, 1};
}

TableCellStyle SheetCell::style(const abstract::Document *document,
                                const abstract::Sheet *,
                                std::uint32_t /*column*/,
                                std::uint32_t /*row*/) const {
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
