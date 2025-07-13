#include <odr/internal/odf/odf_element.hpp>

#include <odr/file.hpp>

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/table_cursor.hpp>
#include <odr/internal/odf/odf_document.hpp>
#include <odr/internal/util/string_util.hpp>
#include <odr/internal/util/xml_util.hpp>

#include <cstring>
#include <optional>
#include <string>

#include <pugixml.hpp>

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

Element::Element(pugi::xml_node node) : m_node{node} {
  if (!node) {
    // TODO log error
    throw std::runtime_error("node not set");
  }
}

ResolvedStyle Element::partial_style(const abstract::Document *document) const {
  if (auto style_name = style_name_(document)) {
    if (auto style = style_(document)->style(style_name)) {
      return style->resolved();
    }
  }
  return {};
}

ResolvedStyle
Element::intermediate_style(const abstract::Document *document) const {
  abstract::Element *parent = this->parent(document);
  if (parent == nullptr) {
    return partial_style(document);
  }
  ResolvedStyle base =
      dynamic_cast<Element *>(parent)->intermediate_style(document);
  base.override(partial_style(document));
  return base;
}

bool Element::is_editable(const abstract::Document *document) const {
  if (m_parent == nullptr) {
    return document_(document)->is_editable();
  }
  return m_parent->is_editable(document);
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

ElementType Root::type(const abstract::Document *) const {
  return ElementType::root;
}

ElementType TextRoot::type(const abstract::Document *) const {
  return ElementType::root;
}

PageLayout TextRoot::page_layout(const abstract::Document *document) const {
  if (auto master_page =
          dynamic_cast<MasterPage *>(this->first_master_page(document))) {
    return master_page->page_layout(document);
  }
  return {};
}

abstract::Element *
TextRoot::first_master_page(const abstract::Document *document) const {
  if (auto first_master_page = style_(document)->first_master_page()) {
    return first_master_page;
  }
  return {};
}

PageLayout MasterPage::page_layout(const abstract::Document *document) const {
  if (auto attribute = m_node.attribute("style:page-layout-name")) {
    return style_(document)->page_layout(attribute.value());
  }
  return {};
}

PageLayout Slide::page_layout(const abstract::Document *document) const {
  if (auto master_page =
          dynamic_cast<MasterPage *>(this->master_page(document))) {
    return master_page->page_layout(document);
  }
  return {};
}

abstract::Element *
Slide::master_page(const abstract::Document *document) const {
  if (auto master_page_name_attr = m_node.attribute("draw:master-page-name")) {
    return style_(document)->master_page(master_page_name_attr.value());
  }
  return {};
}

std::string Slide::name(const abstract::Document *) const {
  return m_node.attribute("draw:name").value();
}

PageLayout Page::page_layout(const abstract::Document *document) const {
  if (auto master_page =
          dynamic_cast<MasterPage *>(this->master_page(document))) {
    return master_page->page_layout(document);
  }
  return {};
}

abstract::Element *Page::master_page(const abstract::Document *document) const {
  if (auto master_page_name_attr = m_node.attribute("draw:master-page-name")) {
    return style_(document)->master_page(master_page_name_attr.value());
  }
  return {};
}

std::string Page::name(const abstract::Document *) const {
  return m_node.attribute("draw:name").value();
}

TextStyle LineBreak::style(const abstract::Document *document) const {
  return intermediate_style(document).text_style;
}

ParagraphStyle Paragraph::style(const abstract::Document *document) const {
  return intermediate_style(document).paragraph_style;
}

TextStyle Paragraph::text_style(const abstract::Document *document) const {
  return intermediate_style(document).text_style;
}

TextStyle Span::style(const abstract::Document *document) const {
  return intermediate_style(document).text_style;
}

Text::Text(pugi::xml_node node) : Text(node, node) {}

Text::Text(pugi::xml_node first, pugi::xml_node last)
    : Element(first), m_last{last} {
  if (!last) {
    // TODO log error
    throw std::runtime_error("last not set");
  }
}

std::string Text::content(const abstract::Document *) const {
  std::string result;
  for (auto node = m_node; node != m_last.next_sibling();
       node = node.next_sibling()) {
    result += text_(node);
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

std::string Text::text_(const pugi::xml_node node) {
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

std::string Link::href(const abstract::Document *) const {
  return m_node.attribute("xlink:href").value();
}

std::string Bookmark::name(const abstract::Document *) const {
  return m_node.attribute("text:name").value();
}

TextStyle ListItem::style(const abstract::Document *document) const {
  return intermediate_style(document).text_style;
}

TableStyle Table::style(const abstract::Document *document) const {
  return partial_style(document).table_style;
}

TableDimensions Table::dimensions(const abstract::Document *) const {
  TableDimensions result;
  TableCursor cursor;

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

TableColumnStyle TableColumn::style(const abstract::Document *document) const {
  return partial_style(document).table_column_style;
}

TableRowStyle TableRow::style(const abstract::Document *document) const {
  return partial_style(document).table_row_style;
}

bool TableCell::is_covered(const abstract::Document *) const {
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

bool Image::is_internal(const abstract::Document *document) const {
  auto doc = document_(document);
  if (!doc || !doc->as_filesystem()) {
    return false;
  }
  try {
    Path path(href(document));
    if (path.relative()) {
      path = Path("/").join(path);
    }
    return doc->as_filesystem()->is_file(path);
  } catch (...) {
  }
  return false;
}

std::optional<odr::File> Image::file(const abstract::Document *document) const {
  auto doc = document_(document);
  if (!doc || !is_internal(document)) {
    return {};
  }
  Path path(href(document));
  if (path.relative()) {
    path = Path("/").join(path);
  }
  return File(doc->as_filesystem()->open(path));
}

std::string Image::href(const abstract::Document *) const {
  return m_node.attribute("xlink:href").value();
}

} // namespace odr::internal::odf
