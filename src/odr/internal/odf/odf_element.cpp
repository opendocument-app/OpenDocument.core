#include <odr/internal/odf/odf_element.hpp>

#include <odr/file.hpp>

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/table_cursor.hpp>
#include <odr/internal/odf/odf_document.hpp>
#include <odr/internal/util/string_util.hpp>
#include <odr/internal/util/xml_util.hpp>

#include <pugixml.hpp>

#include <cstring>
#include <optional>
#include <string>

namespace odr::internal::odf {

namespace {

const char *default_style_name(const pugi::xml_node node) {
  for (auto attribute : node.attributes()) {
    if (util::string::ends_with(attribute.name(), ":style-name")) {
      return attribute.value();
    }
  }
  return {};
}

} // namespace

Element::Element(pugi::xml_node node) : common::Element(node) {}

common::ResolvedStyle
Element::partial_style(const abstract::Document *document) const {
  if (auto style_name = style_name_(document)) {
    if (auto style = style_(document)->style(style_name)) {
      return style->resolved();
    }
  }
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

const char *Element::style_name_(const abstract::Document *) const {
  return default_style_name(m_node);
}

const Document *Element::document_(const abstract::Document *document) {
  return static_cast<const Document *>(document);
}

const StyleRegistry *Element::style_(const abstract::Document *document) {
  return &static_cast<const Document *>(document)->m_style_registry;
}

Root::Root(pugi::xml_node node) : common::Element(node), Element(node) {}

ElementType Root::type(const abstract::Document *) const {
  return ElementType::root;
}

TextRoot::TextRoot(pugi::xml_node node) : common::Element(node), Root(node) {}

ElementType TextRoot::type(const abstract::Document *) const {
  return ElementType::root;
}

PageLayout TextRoot::page_layout(const abstract::Document *document) const {
  if (auto master_page = this->first_master_page(document)) {
    return master_page->page_layout(document);
  }
  return {};
}

abstract::MasterPage *
TextRoot::first_master_page(const abstract::Document *document) const {
  if (auto first_master_page = style_(document)->first_master_page()) {
    return first_master_page;
  }
  return {};
}

PresentationRoot::PresentationRoot(pugi::xml_node node)
    : common::Element(node), Root(node) {}

DrawingRoot::DrawingRoot(pugi::xml_node node)
    : common::Element(node), Root(node) {}

MasterPage::MasterPage(pugi::xml_node node)
    : common::Element(node), Element(node) {}

PageLayout MasterPage::page_layout(const abstract::Document *document) const {
  if (auto attribute = m_node.attribute("style:page-layout-name")) {
    return style_(document)->page_layout(attribute.value());
  }
  return {};
}

Slide::Slide(pugi::xml_node node) : common::Element(node), Element(node) {}

PageLayout Slide::page_layout(const abstract::Document *document) const {
  if (auto master_page = this->master_page(document)) {
    return master_page->page_layout(document);
  }
  return {};
}

abstract::MasterPage *
Slide::master_page(const abstract::Document *document) const {
  if (auto master_page_name_attr = m_node.attribute("draw:master-page-name")) {
    return style_(document)->master_page(master_page_name_attr.value());
  }
  return {};
}

std::string Slide::name(const abstract::Document *) const {
  return m_node.attribute("draw:name").value();
}

Page::Page(pugi::xml_node node) : common::Element(node), Element(node) {}

PageLayout Page::page_layout(const abstract::Document *document) const {
  if (auto master_page = this->master_page(document)) {
    return master_page->page_layout(document);
  }
  return {};
}

abstract::MasterPage *
Page::master_page(const abstract::Document *document) const {
  if (auto master_page_name_attr = m_node.attribute("draw:master-page-name")) {
    return style_(document)->master_page(master_page_name_attr.value());
  }
  return {};
}

std::string Page::name(const abstract::Document *) const {
  return m_node.attribute("draw:name").value();
}

LineBreak::LineBreak(pugi::xml_node node)
    : common::Element(node), Element(node) {}

TextStyle LineBreak::style(const abstract::Document *document) const {
  return intermediate_style(document).text_style;
}

Paragraph::Paragraph(pugi::xml_node node)
    : common::Element(node), Element(node) {}

ParagraphStyle Paragraph::style(const abstract::Document *document) const {
  return intermediate_style(document).paragraph_style;
}

TextStyle Paragraph::text_style(const abstract::Document *document) const {
  return intermediate_style(document).text_style;
}

Span::Span(pugi::xml_node node) : common::Element(node), Element(node) {}

TextStyle Span::style(const abstract::Document *document) const {
  return intermediate_style(document).text_style;
}

std::string Text::text(const pugi::xml_node node) {
  if (node.type() == pugi::node_pcdata) {
    return node.value();
  }

  std::string name = node.name();
  if (name == "text:s") {
    const auto count = node.attribute("text:c").as_uint(1);
    return std::string(count, ' ');
  }
  if (name == "text:tab") {
    return "\t";
  }
  return "";
}

Text::Text(pugi::xml_node node) : Text(node, node) {}

Text::Text(pugi::xml_node first, pugi::xml_node last)
    : common::Element(first), Element(first), m_last{last} {
  if (!last) {
    // TODO log error
    throw std::runtime_error("last not set");
  }
}

std::string Text::content(const abstract::Document *) const {
  std::string result;
  for (auto node = m_node; node != m_last.next_sibling();
       node = node.next_sibling()) {
    result += text(node);
  }
  return result;
}

void Text::set_content(const abstract::Document *, const std::string &text) {
  auto parent = m_node.parent();
  auto old_start = m_node;

  for (auto &&token : util::xml::tokenize_text(text)) {
    switch (token.type) {
    case util::xml::StringToken::Type::none:
      break;
    case util::xml::StringToken::Type::string: {
      auto text_node = parent.insert_child_before(
          pugi::xml_node_type::node_pcdata, old_start);
      text_node.text().set(token.string.c_str());
    } break;
    case util::xml::StringToken::Type::spaces: {
      auto space_node = parent.insert_child_before("text:s", old_start);
      space_node.prepend_attribute("text:c").set_value(token.string.size());
    } break;
    case util::xml::StringToken::Type::tabs: {
      for (std::size_t i = 0; i < token.string.size(); ++i) {
        parent.insert_child_before("text:tab", old_start);
      }
    } break;
    }
  }

  // TODO set first and last
}

TextStyle Text::style(const abstract::Document *document) const {
  return intermediate_style(document).text_style;
}

Link::Link(pugi::xml_node node) : common::Element(node), Element(node) {}

std::string Link::href(const abstract::Document *) const {
  return m_node.attribute("xlink:href").value();
}

Bookmark::Bookmark(pugi::xml_node node)
    : common::Element(node), Element(node) {}

std::string Bookmark::name(const abstract::Document *) const {
  return m_node.attribute("text:name").value();
}

ListItem::ListItem(pugi::xml_node node)
    : common::Element(node), Element(node) {}

TextStyle ListItem::style(const abstract::Document *document) const {
  return intermediate_style(document).text_style;
}

Table::Table(pugi::xml_node node)
    : common::Element(node), Element(node), common::Table(node) {}

TableStyle Table::style(const abstract::Document *document) const {
  return partial_style(document).table_style;
}

TableDimensions
Table::dimensions(const abstract::Document * /*document*/) const {
  TableDimensions result;
  common::TableCursor cursor;

  for (auto column : m_node.children("table:table-column")) {
    const auto columns_repeated =
        column.attribute("table:number-columns-repeated").as_uint(1);
    cursor.add_column(columns_repeated);
  }

  result.columns = cursor.column();
  cursor = {};

  for (auto row : m_node.children("table:table-row")) {
    const auto rows_repeated =
        row.attribute("table:number-rows-repeated").as_uint(1);
    cursor.add_row(rows_repeated);
  }

  result.rows = cursor.row();

  return result;
}

TableColumn::TableColumn(pugi::xml_node node)
    : common::Element(node), Element(node) {}

TableColumnStyle TableColumn::style(const abstract::Document *document) const {
  return partial_style(document).table_column_style;
}

TableRow::TableRow(pugi::xml_node node)
    : common::Element(node), Element(node) {}

TableRowStyle TableRow::style(const abstract::Document *document) const {
  return partial_style(document).table_row_style;
}

TableCell::TableCell(pugi::xml_node node)
    : common::Element(node), Element(node) {}

bool TableCell::covered(const abstract::Document *) const {
  return std::strcmp(m_node.name(), "table:covered-table-cell") == 0;
}

TableDimensions TableCell::span(const abstract::Document *) const {
  return {m_node.attribute("table:number-rows-spanned").as_uint(1),
          m_node.attribute("table:number-columns-spanned").as_uint(1)};
}

ValueType TableCell::value_type(const abstract::Document *) const {
  auto value_type = m_node.attribute("office:value-type").value();
  if (std::strcmp("float", value_type) == 0) {
    return ValueType::float_number;
  }
  return ValueType::string;
}

TableCellStyle TableCell::style(const abstract::Document *document) const {
  return partial_style(document).table_cell_style;
}

Frame::Frame(pugi::xml_node node) : common::Element(node), Element(node) {}

AnchorType Frame::anchor_type(const abstract::Document *) const {
  auto anchor_type = m_node.attribute("text:anchor-type").value();
  if (std::strcmp("as-char", anchor_type) == 0) {
    return AnchorType::as_char;
  }
  if (std::strcmp("char", anchor_type) == 0) {
    return AnchorType::at_char;
  }
  if (std::strcmp("paragraph", anchor_type) == 0) {
    return AnchorType::at_paragraph;
  }
  if (std::strcmp("page", anchor_type) == 0) {
    return AnchorType::at_page;
  }
  return AnchorType::at_page;
}

std::optional<std::string> Frame::x(const abstract::Document *) const {
  if (auto attribute = m_node.attribute("svg:x")) {
    return attribute.value();
  }
  return {};
}

std::optional<std::string> Frame::y(const abstract::Document *) const {
  if (auto attribute = m_node.attribute("svg:y")) {
    return attribute.value();
  }
  return {};
}

std::optional<std::string> Frame::width(const abstract::Document *) const {
  if (auto attribute = m_node.attribute("svg:width")) {
    return attribute.value();
  }
  return {};
}

std::optional<std::string> Frame::height(const abstract::Document *) const {
  if (auto attribute = m_node.attribute("svg:height")) {
    return attribute.value();
  }
  return {};
}

std::optional<std::string> Frame::z_index(const abstract::Document *) const {
  if (auto attribute = m_node.attribute("draw:z-index")) {
    return attribute.value();
  }
  return {};
}

GraphicStyle Frame::style(const abstract::Document *document) const {
  return intermediate_style(document).graphic_style;
}

Rect::Rect(pugi::xml_node node) : common::Element(node), Element(node) {}

std::string Rect::x(const abstract::Document *) const {
  return m_node.attribute("svg:x").value();
}

std::string Rect::y(const abstract::Document *) const {
  return m_node.attribute("svg:y").value();
}

std::string Rect::width(const abstract::Document *) const {
  return m_node.attribute("svg:width").value();
}

std::string Rect::height(const abstract::Document *) const {
  return m_node.attribute("svg:height").value();
}

GraphicStyle Rect::style(const abstract::Document *document) const {
  return intermediate_style(document).graphic_style;
}

Line::Line(pugi::xml_node node) : common::Element(node), Element(node) {}

std::string Line::x1(const abstract::Document *) const {
  return m_node.attribute("svg:x1").value();
}

std::string Line::y1(const abstract::Document *) const {
  return m_node.attribute("svg:y1").value();
}

std::string Line::x2(const abstract::Document *) const {
  return m_node.attribute("svg:x2").value();
}

std::string Line::y2(const abstract::Document *) const {
  return m_node.attribute("svg:y2").value();
}

GraphicStyle Line::style(const abstract::Document *document) const {
  return intermediate_style(document).graphic_style;
}

Circle::Circle(pugi::xml_node node) : common::Element(node), Element(node) {}

std::string Circle::x(const abstract::Document *) const {
  return m_node.attribute("svg:x").value();
}

std::string Circle::y(const abstract::Document *) const {
  return m_node.attribute("svg:y").value();
}

std::string Circle::width(const abstract::Document *) const {
  return m_node.attribute("svg:width").value();
}

std::string Circle::height(const abstract::Document *) const {
  return m_node.attribute("svg:height").value();
}

GraphicStyle Circle::style(const abstract::Document *document) const {
  return intermediate_style(document).graphic_style;
}

CustomShape::CustomShape(pugi::xml_node node)
    : common::Element(node), Element(node) {}

std::optional<std::string> CustomShape::x(const abstract::Document *) const {
  return m_node.attribute("svg:x").value();
}

std::optional<std::string> CustomShape::y(const abstract::Document *) const {
  return m_node.attribute("svg:y").value();
}

std::string CustomShape::width(const abstract::Document *) const {
  return m_node.attribute("svg:width").value();
}

std::string CustomShape::height(const abstract::Document *) const {
  return m_node.attribute("svg:height").value();
}

GraphicStyle CustomShape::style(const abstract::Document *document) const {
  return intermediate_style(document).graphic_style;
}

Image::Image(pugi::xml_node node) : common::Element(node), Element(node) {}

bool Image::internal(const abstract::Document *document) const {
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

std::optional<odr::File> Image::file(const abstract::Document *document) const {
  auto doc = document_(document);
  if (!doc || !internal(document)) {
    return {};
  }
  return File(doc->files()->open(href(document)));
}

std::string Image::href(const abstract::Document *) const {
  return m_node.attribute("xlink:href").value();
}

} // namespace odr::internal::odf
