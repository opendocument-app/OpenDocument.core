#include <odr/internal/ooxml/presentation/ooxml_presentation_element.hpp>

#include <odr/file.hpp>

#include <odr/internal/ooxml/ooxml_util.hpp>
#include <odr/internal/ooxml/presentation/ooxml_presentation_document.hpp>

#include <pugixml.hpp>

namespace odr::internal::ooxml::presentation {

namespace {

void resolve_text_style_(pugi::xml_node node, TextStyle &result) {
  auto run_properties = node.child("a:rPr");

  if (auto font_name = run_properties.child("rFonts").attribute("ascii")) {
    result.font_name = font_name.value();
  }
  if (auto font_size =
          read_hundredth_point_attribute(run_properties.attribute("sz"))) {
    result.font_size = font_size;
  }
  if (auto font_weight =
          read_font_weight_attribute(run_properties.attribute("b"))) {
    result.font_weight = font_weight;
  }
  if (auto font_style =
          read_font_style_attribute(run_properties.attribute("i"))) {
    result.font_style = font_style;
  }
  if (auto font_underline =
          read_line_attribute(run_properties.attribute("u"))) {
    result.font_underline = font_underline;
  }
  if (auto font_line_through =
          read_line_attribute(run_properties.attribute("strike"))) {
    result.font_line_through = font_line_through;
  }
  if (auto font_shadow =
          read_shadow_attribute(run_properties.attribute("shadow"))) {
    result.font_shadow = font_shadow;
  }
  if (auto font_color =
          read_color_attribute(run_properties.attribute("color"))) {
    result.font_color = font_color;
  }
  if (auto background_color =
          read_color_attribute(run_properties.attribute("highlight"))) {
    result.background_color = background_color;
  }
}

void resolve_paragraph_style_(pugi::xml_node node, ParagraphStyle &result) {
  auto paragraph_properties = node.child("a:pPr");

  if (auto text_align =
          read_text_align_attribute(paragraph_properties.attribute("jc"))) {
    result.text_align = text_align;
  }
  if (auto margin_left = read_twips_attribute(
          paragraph_properties.child("ind").attribute("left"))) {
    result.margin.left = margin_left;
  }
  if (auto margin_left = read_twips_attribute(
          paragraph_properties.child("ind").attribute("start"))) {
    result.margin.left = margin_left;
  }
  if (auto margin_right = read_twips_attribute(
          paragraph_properties.child("ind").attribute("right"))) {
    result.margin.right = margin_right;
  }
  if (auto margin_right = read_twips_attribute(
          paragraph_properties.child("ind").attribute("end"))) {
    result.margin.right = margin_right;
  }
}

} // namespace

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

pugi::xml_node Element::slide_(const abstract::Document *document,
                               const std::string &id) {
  return document_(document)->m_slides_xml.at(id).document_element();
}

PageLayout Slide::page_layout(const abstract::Document *) const {
  return {}; // TODO
}

abstract::Element *Slide::master_page(const abstract::Document *) const {
  return {}; // TODO
}

std::string Slide::name(const abstract::Document *) const {
  return {}; // TODO
}

pugi::xml_node Slide::slide_node_(const abstract::Document *document) const {
  return slide_(document, m_node.attribute("r:id").value());
}

common::ResolvedStyle
Paragraph::partial_style(const abstract::Document *) const {
  common::ResolvedStyle result;
  resolve_text_style_(m_node, result.text_style);
  resolve_paragraph_style_(m_node, result.paragraph_style);
  return result;
}

ParagraphStyle Paragraph::style(const abstract::Document *document) const {
  return partial_style(document).paragraph_style;
}

TextStyle Paragraph::text_style(const abstract::Document *document) const {
  return partial_style(document).text_style;
}

common::ResolvedStyle Span::partial_style(const abstract::Document *) const {
  common::ResolvedStyle result;
  resolve_text_style_(m_node, result.text_style);
  return result;
}

TextStyle Span::style(const abstract::Document *document) const {
  return partial_style(document).text_style;
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

  if (name == "a:t") {
    return node.text().get();
  }
  if (name == "a:tab") {
    return "\t";
  }

  return "";
}

TableDimensions TableElement::dimensions(const abstract::Document *) const {
  return {}; // TODO
}

abstract::Element *
TableElement::first_column(const abstract::Document *) const {
  return {}; // TODO
}

abstract::Element *TableElement::first_row(const abstract::Document *) const {
  return {}; // TODO
}

TableStyle TableElement::style(const abstract::Document *document) const {
  return partial_style(document).table_style;
}

TableColumnStyle TableColumn::style(const abstract::Document *document) const {
  return partial_style(document).table_column_style;
}

TableRowStyle TableRow::style(const abstract::Document *document) const {
  return partial_style(document).table_row_style;
}

bool TableCell::is_covered(const abstract::Document *) const { return false; }

TableDimensions TableCell::span(const abstract::Document *) const {
  return {1, 1}; // TODO
}

ValueType TableCell::value_type(const abstract::Document *) const {
  return ValueType::string;
}

TableCellStyle TableCell::style(const abstract::Document *document) const {
  return partial_style(document).table_cell_style;
}

AnchorType Frame::anchor_type(const abstract::Document *) const {
  return AnchorType::at_page;
}

std::optional<std::string> Frame::x(const abstract::Document *) const {
  if (auto x = read_emus_attribute(
          m_node.child("p:spPr").child("a:xfrm").child("a:off").attribute(
              "x"))) {
    return x->to_string();
  }
  return {};
}

std::optional<std::string> Frame::y(const abstract::Document *) const {
  if (auto y = read_emus_attribute(
          m_node.child("p:spPr").child("a:xfrm").child("a:off").attribute(
              "y"))) {
    return y->to_string();
  }
  return {};
}

std::optional<std::string> Frame::width(const abstract::Document *) const {
  if (auto cx = read_emus_attribute(
          m_node.child("p:spPr").child("a:xfrm").child("a:ext").attribute(
              "cx"))) {
    return cx->to_string();
  }
  return {};
}

std::optional<std::string> Frame::height(const abstract::Document *) const {
  if (auto cy = read_emus_attribute(
          m_node.child("p:spPr").child("a:xfrm").child("a:ext").attribute(
              "cy"))) {
    return cy->to_string();
  }
  return {};
}

std::optional<std::string> Frame::z_index(const abstract::Document *) const {
  return {}; // TODO
}

GraphicStyle Frame::style(const abstract::Document *) const {
  return {}; // TODO
}

bool ImageElement::is_internal(const abstract::Document *) const {
  return false;
}

std::optional<File> ImageElement::file(const abstract::Document *) const {
  return {}; // TODO
}

std::string ImageElement::href(const abstract::Document *) const {
  return ""; // TODO
}

} // namespace odr::internal::ooxml::presentation
