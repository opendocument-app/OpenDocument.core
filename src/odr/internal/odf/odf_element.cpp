#include <cstring>
#include <functional>
#include <odr/file.hpp>
#include <odr/internal/abstract/document.hpp>
#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/document_element.hpp>
#include <odr/internal/common/style.hpp>
#include <odr/internal/common/table_cursor.hpp>
#include <odr/internal/odf/odf_document.hpp>
#include <odr/internal/odf/odf_element.hpp>
#include <odr/internal/odf/odf_style.hpp>
#include <odr/internal/util/string_util.hpp>
#include <odr/internal/util/xml_util.hpp>
#include <odr/style.hpp>
#include <optional>
#include <pugixml.hpp>
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

Element::Element() = default;

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

PageLayout MasterPage::page_layout(const abstract::Document *document) const {
  if (auto attribute = m_node.attribute("style:page-layout-name")) {
    return style_(document)->page_layout(attribute.value());
  }
  return {};
}

ElementType Root::type(const abstract::Document *) const {
  return ElementType::root;
}

ElementType TextRoot::type(const abstract::Document *) const {
  return ElementType::root;
}

PageLayout TextRoot::page_layout(const abstract::Document *document) const {
  if (auto first_master_page = style_(document)->first_master_page()) {
    auto master_page_node =
        style_(document)->master_page_node(*first_master_page);
    return MasterPage(master_page_node).page_layout(document);
  }
  return {};
}

abstract::Element *
TextRoot::first_master_page(const abstract::Document *) const {
  return nullptr; // TODO
}

PageLayout Slide::page_layout(const abstract::Document *document) const {
  if (auto master_page_node = master_page_node_(document)) {
    return MasterPage(master_page_node).page_layout(document);
  }
  return {};
}

abstract::Element *
Slide::master_page(const abstract::Document *document) const {
  if (auto master_page_node = master_page_node_(document)) {
    // TODO
  }
  return {};
}

std::string Slide::name(const abstract::Document *) const {
  return m_node.attribute("draw:name").value();
}

pugi::xml_node
Slide::master_page_node_(const abstract::Document *document) const {
  if (auto master_page_name_attr = m_node.attribute("draw:master-page-name")) {
    return style_(document)->master_page_node(master_page_name_attr.value());
  }
  return {};
}

std::string Sheet::name(const abstract::Document *) const {
  return m_node.attribute("table:name").value();
}

TableDimensions Sheet::dimensions(const abstract::Document *document) const {
  return {}; // TODO
}

TableDimensions
Sheet::content(const abstract::Document *,
               const std::optional<TableDimensions> range) const {
  TableDimensions result;

  common::TableCursor cursor;
  for (auto row : m_node.children("table:table-row")) {
    const auto rows_repeated =
        row.attribute("table:number-rows-repeated").as_uint(1);
    cursor.add_row(rows_repeated);

    for (auto cell : row.children("table:table-cell")) {
      const auto columns_repeated =
          cell.attribute("table:number-columns-repeated").as_uint(1);
      const auto colspan =
          cell.attribute("table:number-columns-spanned").as_uint(1);
      const auto rowspan =
          cell.attribute("table:number-rows-spanned").as_uint(1);
      cursor.add_cell(colspan, rowspan, columns_repeated);

      const auto new_rows = cursor.row();
      const auto new_cols = std::max(result.columns, cursor.column());
      if (cell.first_child() &&
          (range && (new_rows < range->rows) && (new_cols < range->columns))) {
        result.rows = new_rows;
        result.columns = new_cols;
      }
    }
  }

  return result;
}

abstract::Element *Sheet::column(const abstract::Document *document,
                                 std::uint32_t column) const {
  return nullptr; // TODO
}

abstract::Element *Sheet::row(const abstract::Document *document,
                              std::uint32_t column) const {
  return nullptr; // TODO
}

abstract::Element *Sheet::cell(const abstract::Document *document,
                               std::uint32_t column) const {
  return nullptr; // TODO
}

abstract::Element *Sheet::first_shape(const abstract::Document *) const {
  return nullptr; // TODO
}

TableStyle Sheet::style(const abstract::Document *document) const {
  return {}; // TODO
}

PageLayout Page::page_layout(const abstract::Document *document) const {
  if (auto master_page_node = master_page_node_(document)) {
    return MasterPage(master_page_node).page_layout(document);
  }
  return {};
}

abstract::Element *Page::master_page(const abstract::Document *document) const {
  if (auto master_page_node = master_page_node_(document)) {
    // TODO
  }
  return {};
}

std::string Page::name(const abstract::Document *) const {
  return m_node.attribute("draw:name").value();
}

pugi::xml_node
Page::master_page_node_(const abstract::Document *document) const {
  if (auto master_page_name_attr = m_node.attribute("draw:master-page-name")) {
    return style_(document)->master_page_node(master_page_name_attr.value());
  }
  return {};
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

Text::Text() = default;

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

std::string Link::href(const abstract::Document *) const {
  return m_node.attribute("xlink:href").value();
}

std::string Bookmark::name(const abstract::Document *) const {
  return m_node.attribute("text:name").value();
}

TextStyle ListItem::style(const abstract::Document *document) const {
  return intermediate_style(document).text_style;
}

TableStyle TableElement::style(const abstract::Document *document) const {
  return partial_style(document).table_style;
}

TableDimensions TableElement::dimensions(const abstract::Document *) const {
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

abstract::Element *TableElement::first_row(const abstract::Document *) const {
  return nullptr;
}

abstract::Element *
TableElement::first_column(const abstract::Document *) const {
  return nullptr;
}

TableColumnStyle TableColumn::style(const abstract::Document *document) const {
  return partial_style(document).table_column_style;
}

TableRowStyle TableRow::style(const abstract::Document *document) const {
  return partial_style(document).table_row_style;
}

abstract::Element *TableCell::column(const abstract::Document *) const {
  return nullptr;
}

abstract::Element *TableCell::row(const abstract::Document *) const {
  return nullptr;
}

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
  if (auto attribute = m_node.attribute("svg:height")) {
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

std::string ImageElement::href(const abstract::Document *) const {
  return m_node.attribute("xlink:href").value();
}

} // namespace odr::internal::odf
