#include <odr/file.hpp>
#include <odr/internal/abstract/document.hpp>
#include <odr/internal/common/document_element.hpp>
#include <odr/internal/common/style.hpp>
#include <odr/internal/ooxml/ooxml_util.hpp>
#include <odr/internal/ooxml/presentation/ooxml_presentation_document.hpp>
#include <odr/internal/ooxml/presentation/ooxml_presentation_element.hpp>
#include <odr/quantity.hpp>
#include <odr/style.hpp>
#include <pugixml.hpp>

namespace odr::internal::ooxml::presentation {

Element::Element(pugi::xml_node node) : m_node{node} {
  if (!node) {
    // TODO log error
    throw std::runtime_error("node not set");
  }
}

common::ResolvedStyle Element::partial_style(const abstract::Document *) const {
  return {}; // TODO
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

pugi::xml_node Element::slide_(const abstract::Document *document,
                               const std::string &id) {
  return document_(document)->m_slides_xml.at(id).document_element();
}

PageLayout Slide::page_layout(const abstract::Document *,
                              ElementIdentifier) const {
  return {}; // TODO
}

std::pair<abstract::Element *, ElementIdentifier>
Slide::master_page(const abstract::Document *, ElementIdentifier) const {
  return {}; // TODO
}

std::string Slide::name(const abstract::Document *, ElementIdentifier) const {
  return {}; // TODO
}

pugi::xml_node Slide::slide_node_(const abstract::Document *document) const {
  return slide_(document, m_node.attribute("r:id").value());
}

ParagraphStyle Paragraph::style(const abstract::Document *document,
                                ElementIdentifier) const {
  return partial_style(document).paragraph_style;
}

TextStyle Paragraph::text_style(const abstract::Document *document,
                                ElementIdentifier) const {
  return partial_style(document).text_style;
}

TextStyle Span::style(const abstract::Document *document,
                      ElementIdentifier) const {
  return partial_style(document).text_style;
}

Text::Text(pugi::xml_node node) : Text(node, node) {}

Text::Text(pugi::xml_node first, pugi::xml_node last)
    : Element(first), m_last{last} {}

std::string Text::content(const abstract::Document *, ElementIdentifier) const {
  std::string result;
  for (auto node = m_node; node != m_last.next_sibling();
       node = node.next_sibling()) {
    result += text_(node);
  }
  return result;
}

void Text::set_content(const abstract::Document *, ElementIdentifier,
                       const std::string &) {
  // TODO
}

TextStyle Text::style(const abstract::Document *document,
                      ElementIdentifier) const {
  return partial_style(document).text_style;
}

std::string Text::text_(const pugi::xml_node node) {
  std::string name = node.name();

  if (name == "a:t") {
    return node.text().get();
  }
  if (name == "a:tab") {
    return "\t";
  }

  return "";
}

TableDimensions TableElement::dimensions(const abstract::Document *,
                                         ElementIdentifier) const {
  return {}; // TODO
}

std::pair<abstract::Element *, ElementIdentifier>
TableElement::first_column(const abstract::Document *,
                           ElementIdentifier) const {
  return {}; // TODO
}

std::pair<abstract::Element *, ElementIdentifier>
TableElement::first_row(const abstract::Document *, ElementIdentifier) const {
  return {}; // TODO
}

TableStyle TableElement::style(const abstract::Document *document,
                               ElementIdentifier) const {
  return partial_style(document).table_style;
}

TableColumnStyle TableColumn::style(const abstract::Document *document,
                                    ElementIdentifier) const {
  return partial_style(document).table_column_style;
}

TableRowStyle TableRow::style(const abstract::Document *document,
                              ElementIdentifier) const {
  return partial_style(document).table_row_style;
}

bool TableCell::covered(const abstract::Document *, ElementIdentifier) const {
  return false;
}

TableDimensions TableCell::span(const abstract::Document *,
                                ElementIdentifier) const {
  return {1, 1}; // TODO
}

ValueType TableCell::value_type(const abstract::Document *,
                                ElementIdentifier) const {
  return ValueType::string;
}

TableCellStyle TableCell::style(const abstract::Document *document,
                                ElementIdentifier) const {
  return partial_style(document).table_cell_style;
}

AnchorType Frame::anchor_type(const abstract::Document *,
                              ElementIdentifier) const {
  return AnchorType::at_page;
}

std::optional<std::string> Frame::x(const abstract::Document *,
                                    ElementIdentifier) const {
  if (auto x = read_emus_attribute(
          m_node.child("p:spPr").child("a:xfrm").child("a:off").attribute(
              "x"))) {
    return x->to_string();
  }
  return {};
}

std::optional<std::string> Frame::y(const abstract::Document *,
                                    ElementIdentifier) const {
  if (auto y = read_emus_attribute(
          m_node.child("p:spPr").child("a:xfrm").child("a:off").attribute(
              "y"))) {
    return y->to_string();
  }
  return {};
}

std::optional<std::string> Frame::width(const abstract::Document *,
                                        ElementIdentifier) const {
  if (auto cx = read_emus_attribute(
          m_node.child("p:spPr").child("a:xfrm").child("a:ext").attribute(
              "cx"))) {
    return cx->to_string();
  }
  return {};
}

std::optional<std::string> Frame::height(const abstract::Document *,
                                         ElementIdentifier) const {
  if (auto cy = read_emus_attribute(
          m_node.child("p:spPr").child("a:xfrm").child("a:ext").attribute(
              "cy"))) {
    return cy->to_string();
  }
  return {};
}

std::optional<std::string> Frame::z_index(const abstract::Document *,
                                          ElementIdentifier) const {
  return {}; // TODO
}

GraphicStyle Frame::style(const abstract::Document *, ElementIdentifier) const {
  return {}; // TODO
}

bool ImageElement::internal(const abstract::Document *,
                            ElementIdentifier) const {
  return false;
}

std::optional<odr::File> ImageElement::file(const abstract::Document *,
                                            ElementIdentifier) const {
  return {}; // TODO
}

std::string ImageElement::href(const abstract::Document *,
                               ElementIdentifier) const {
  return ""; // TODO
}

} // namespace odr::internal::ooxml::presentation
