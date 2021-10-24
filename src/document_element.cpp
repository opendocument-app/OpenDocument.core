#include <internal/abstract/document_element.h>
#include <odr/document_element.h>
#include <odr/file.h>
#include <odr/style.h>

namespace odr {

Element::Element(const internal::abstract::Document *document,
                 const internal::abstract::DocumentCursor *cursor,
                 internal::abstract::Element *element)
    : m_document{document}, m_cursor{cursor}, m_element{element} {}

bool Element::operator==(const Element &rhs) const {
  return m_element->equals(*rhs.m_element);
}

bool Element::operator!=(const Element &rhs) const { return !operator==(rhs); }

Element::operator bool() const { return m_element; }

ElementType Element::type() const { return m_element->type(m_document); }

TextRoot Element::text_root() const {
  return {m_document, m_cursor, m_element};
}

Slide Element::slide() const { return {m_document, m_cursor, m_element}; }

Sheet Element::sheet() const { return {m_document, m_cursor, m_element}; }

Page Element::page() const { return {m_document, m_cursor, m_element}; }

LineBreak Element::line_break() const {
  return {m_document, m_cursor, m_element};
}

Paragraph Element::paragraph() const {
  return {m_document, m_cursor, m_element};
}

Span Element::span() const { return {m_document, m_cursor, m_element}; }

Text Element::text() const { return {m_document, m_cursor, m_element}; }

Link Element::link() const { return {m_document, m_cursor, m_element}; }

Bookmark Element::bookmark() const { return {m_document, m_cursor, m_element}; }

ListItem Element::list_item() const {
  return {m_document, m_cursor, m_element};
}

Table Element::table() const { return {m_document, m_cursor, m_element}; }

TableColumn Element::table_column() const {
  return {m_document, m_cursor, m_element};
}

TableRow Element::table_row() const {
  return {m_document, m_cursor, m_element};
}

TableCell Element::table_cell() const {
  return {m_document, m_cursor, m_element};
}

Frame Element::frame() const { return {m_document, m_cursor, m_element}; }

Rect Element::rect() const { return {m_document, m_cursor, m_element}; }

Line Element::line() const { return {m_document, m_cursor, m_element}; }

Circle Element::circle() const { return {m_document, m_cursor, m_element}; }

CustomShape Element::custom_shape() const {
  return {m_document, m_cursor, m_element};
}

Image Element::image() const { return {m_document, m_cursor, m_element}; }

PageLayout TextRoot::page_layout() const {
  return m_element ? m_element->page_layout(m_document) : PageLayout();
}

std::string Slide::name() const {
  return m_element ? m_element->name(m_document) : "";
}

PageLayout Slide::page_layout() const {
  return m_element ? m_element->page_layout(m_document) : PageLayout();
}

std::string Sheet::name() const {
  return m_element ? m_element->name(m_document) : "";
}

TableDimensions Sheet::content(std::optional<TableDimensions> range) const {
  return m_element ? m_element->content(m_document, range) : TableDimensions();
}

std::string Page::name() const {
  return m_element ? m_element->name(m_document) : "";
}

PageLayout Page::page_layout() const {
  return m_element ? m_element->page_layout(m_document) : PageLayout();
}

std::optional<TextStyle> LineBreak::style() const {
  return m_element ? m_element->style(m_document, m_cursor)
                   : std::optional<TextStyle>();
}

std::optional<ParagraphStyle> Paragraph::style() const {
  return m_element ? m_element->style(m_document, m_cursor)
                   : std::optional<ParagraphStyle>();
}

std::optional<TextStyle> Paragraph::text_style() const {
  return m_element ? m_element->text_style(m_document, m_cursor)
                   : std::optional<TextStyle>();
}

std::optional<TextStyle> Span::style() const {
  return m_element ? m_element->style(m_document, m_cursor)
                   : std::optional<TextStyle>();
}

std::string Text::content() const {
  return m_element ? m_element->content(m_document) : "";
}

void Text::set_content(const std::string &text) const {
  if (m_element) {
    m_element->set_content(m_document, text);
  }
}

std::optional<TextStyle> Text::style() const {
  return m_element ? m_element->style(m_document, m_cursor)
                   : std::optional<TextStyle>();
}

std::string Link::href() const {
  return m_element ? m_element->href(m_document) : "";
}

std::string Bookmark::name() const {
  return m_element ? m_element->name(m_document) : "";
}

std::optional<TextStyle> ListItem::style() const {
  return m_element ? m_element->style(m_document, m_cursor)
                   : std::optional<TextStyle>();
}

TableDimensions Table::dimensions() const {
  return m_element ? m_element->dimensions(m_document) : TableDimensions();
}

std::optional<TableStyle> Table::style() const {
  return m_element ? m_element->style(m_document, m_cursor)
                   : std::optional<TableStyle>();
}

std::optional<TableColumnStyle> TableColumn::style() const {
  return m_element ? m_element->style(m_document, m_cursor)
                   : std::optional<TableColumnStyle>();
}

std::optional<TableRowStyle> TableRow::style() const {
  return m_element ? m_element->style(m_document, m_cursor)
                   : std::optional<TableRowStyle>();
}

TableColumn TableCell::column() const {
  return m_element ? TableColumn(m_document, m_cursor,
                                 m_element->column(m_document, m_cursor))
                   : TableColumn();
}

TableRow TableCell::row() const {
  return m_element ? TableRow(m_document, m_cursor,
                              m_element->row(m_document, m_cursor))
                   : TableRow();
}

bool TableCell::covered() const {
  return m_element && m_element->covered(m_document);
}

TableDimensions TableCell::span() const {
  return m_element ? m_element->span(m_document) : TableDimensions();
}

ValueType TableCell::value_type() const {
  return m_element ? m_element->value_type(m_document) : ValueType::string;
}

std::optional<TableCellStyle> TableCell::style() const {
  return m_element ? m_element->style(m_document, m_cursor)
                   : std::optional<TableCellStyle>();
}

AnchorType Frame::anchor_type() const {
  return m_element ? m_element->anchor_type(m_document)
                   : AnchorType::as_char; // TODO default?
}

std::optional<std::string> Frame::x() const {
  return m_element ? m_element->x(m_document) : std::optional<std::string>();
}

std::optional<std::string> Frame::y() const {
  return m_element ? m_element->y(m_document) : std::optional<std::string>();
}

std::optional<std::string> Frame::width() const {
  return m_element ? m_element->width(m_document)
                   : std::optional<std::string>();
}

std::optional<std::string> Frame::height() const {
  return m_element ? m_element->height(m_document)
                   : std::optional<std::string>();
}

std::optional<std::string> Frame::z_index() const {
  return m_element ? m_element->z_index(m_document)
                   : std::optional<std::string>();
}

std::optional<GraphicStyle> Frame::style() const {
  return m_element ? m_element->style(m_document, m_cursor)
                   : std::optional<GraphicStyle>();
}

std::string Rect::x() const {
  return m_element ? m_element->x(m_document) : "";
}

std::string Rect::y() const {
  return m_element ? m_element->y(m_document) : "";
}

std::string Rect::width() const {
  return m_element ? m_element->width(m_document) : "";
}

std::string Rect::height() const {
  return m_element ? m_element->height(m_document) : "";
}

std::optional<GraphicStyle> Rect::style() const {
  return m_element ? m_element->style(m_document, m_cursor)
                   : std::optional<GraphicStyle>();
}

std::string Line::x1() const {
  return m_element ? m_element->x1(m_document) : "";
}

std::string Line::y1() const {
  return m_element ? m_element->y1(m_document) : "";
}

std::string Line::x2() const {
  return m_element ? m_element->x2(m_document) : "";
}

std::string Line::y2() const {
  return m_element ? m_element->y2(m_document) : "";
}

std::optional<GraphicStyle> Line::style() const {
  return m_element ? m_element->style(m_document, m_cursor)
                   : std::optional<GraphicStyle>();
}

std::string Circle::x() const {
  return m_element ? m_element->x(m_document) : "";
}

std::string Circle::y() const {
  return m_element ? m_element->y(m_document) : "";
}

std::string Circle::width() const {
  return m_element ? m_element->width(m_document) : "";
}

std::string Circle::height() const {
  return m_element ? m_element->height(m_document) : "";
}

std::optional<GraphicStyle> Circle::style() const {
  return m_element ? m_element->style(m_document, m_cursor)
                   : std::optional<GraphicStyle>();
}

std::optional<std::string> CustomShape::x() const {
  return m_element ? m_element->x(m_document) : "";
}

std::optional<std::string> CustomShape::y() const {
  return m_element ? m_element->y(m_document) : "";
}

std::string CustomShape::width() const {
  return m_element ? m_element->width(m_document) : "";
}

std::string CustomShape::height() const {
  return m_element ? m_element->height(m_document) : "";
}

std::optional<GraphicStyle> CustomShape::style() const {
  return m_element ? m_element->style(m_document, m_cursor)
                   : std::optional<GraphicStyle>();
}

bool Image::internal() const {
  return m_element && m_element->internal(m_document);
}

std::optional<File> Image::file() const {
  return m_element ? m_element->file(m_document) : std::optional<File>();
}

std::string Image::href() const {
  return m_element ? m_element->href(m_document) : "";
}

} // namespace odr
