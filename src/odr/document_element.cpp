#include <odr/document_element.hpp>
#include <odr/file.hpp>
#include <odr/internal/abstract/document_element.hpp>
#include <odr/style.hpp>

namespace odr {

Element::Element() = default;

Element::Element(const internal::abstract::Document *document,
                 internal::abstract::Element *element)
    : m_document{document}, m_element{element} {}

bool Element::operator==(const Element &rhs) const {
  return m_element == rhs.m_element;
}

bool Element::operator!=(const Element &rhs) const {
  return m_element != rhs.m_element;
}

Element::operator bool() const { return m_element; }

ElementType Element::type() const {
  return m_element ? m_element->type(m_document) : ElementType::none;
}

Element Element::parent() const {
  return m_element ? Element(m_document, m_element->parent(m_document))
                   : Element();
}

Element Element::first_child() const {
  return m_element ? Element(m_document, m_element->first_child(m_document))
                   : Element();
}

Element Element::previous_sibling() const {
  return m_element
             ? Element(m_document, m_element->previous_sibling(m_document))
             : Element();
}

Element Element::next_sibling() const {
  return m_element ? Element(m_document, m_element->next_sibling(m_document))
                   : Element();
}

TextRoot Element::text_root() const { return {m_document, m_element}; }

Slide Element::slide() const { return {m_document, m_element}; }

Sheet Element::sheet() const { return {m_document, m_element}; }

Page Element::page() const { return {m_document, m_element}; }

MasterPage Element::master_page() const { return {m_document, m_element}; }

LineBreak Element::line_break() const { return {m_document, m_element}; }

Paragraph Element::paragraph() const { return {m_document, m_element}; }

Span Element::span() const { return {m_document, m_element}; }

Text Element::text() const { return {m_document, m_element}; }

Link Element::link() const { return {m_document, m_element}; }

Bookmark Element::bookmark() const { return {m_document, m_element}; }

ListItem Element::list_item() const { return {m_document, m_element}; }

Table Element::table() const { return {m_document, m_element}; }

TableColumn Element::table_column() const { return {m_document, m_element}; }

TableRow Element::table_row() const { return {m_document, m_element}; }

TableCell Element::table_cell() const { return {m_document, m_element}; }

Frame Element::frame() const { return {m_document, m_element}; }

Rect Element::rect() const { return {m_document, m_element}; }

Line Element::line() const { return {m_document, m_element}; }

Circle Element::circle() const { return {m_document, m_element}; }

CustomShape Element::custom_shape() const { return {m_document, m_element}; }

Image Element::image() const { return {m_document, m_element}; }

ElementIterator Element::begin() const {
  return m_element
             ? ElementIterator(m_document, m_element->first_child(m_document))
             : ElementIterator();
}

ElementIterator Element::end() const { return {}; }

ElementIterator::ElementIterator() = default;

ElementIterator::ElementIterator(const internal::abstract::Document *document,
                                 internal::abstract::Element *element)
    : m_document{document}, m_element{element} {}

bool ElementIterator::operator==(const ElementIterator &rhs) const {
  return m_element == rhs.m_element;
}

bool ElementIterator::operator!=(const ElementIterator &rhs) const {
  return m_element != rhs.m_element;
}

ElementIterator::reference ElementIterator::operator*() const {
  return Element(m_document, m_element);
}

ElementIterator &ElementIterator::operator++() {
  if (m_element != nullptr) {
    m_element = m_element->next_sibling(m_document);
  }
  return *this;
}

ElementIterator ElementIterator::operator++(int) {
  if (m_element == nullptr) {
    return {};
  }
  return {m_document, m_element->next_sibling(m_document)};
}

ElementRange::ElementRange() = default;

ElementRange::ElementRange(ElementIterator begin) : m_begin{std::move(begin)} {}

ElementRange::ElementRange(ElementIterator begin, ElementIterator end)
    : m_begin{std::move(begin)}, m_end{std::move(end)} {}

ElementIterator ElementRange::begin() const { return m_begin; }

ElementIterator ElementRange::end() const { return m_end; }

PageLayout TextRoot::page_layout() const {
  return m_element ? m_element->page_layout(m_document) : PageLayout();
}

MasterPage TextRoot::first_master_page() const {
  return m_element
             ? MasterPage(m_document, m_element->first_master_page(m_document))
             : MasterPage();
}

std::string Slide::name() const {
  return m_element ? m_element->name(m_document) : "";
}

PageLayout Slide::page_layout() const {
  return m_element ? m_element->page_layout(m_document) : PageLayout();
}

MasterPage Slide::master_page() const {
  return m_element ? MasterPage(m_document, m_element->master_page(m_document))
                   : MasterPage();
}

std::string Sheet::name() const {
  return m_element ? m_element->name(m_document) : "";
}

TableDimensions Sheet::content(std::optional<TableDimensions> range) const {
  return m_element ? m_element->content(m_document, range) : TableDimensions();
}

ElementRange Sheet::shapes() const {
  return m_element ? ElementRange(ElementIterator(
                         m_document, m_element->first_shape(m_document)))
                   : ElementRange();
}

std::string Page::name() const {
  return m_element ? m_element->name(m_document) : "";
}

PageLayout Page::page_layout() const {
  return m_element ? m_element->page_layout(m_document) : PageLayout();
}

MasterPage Page::master_page() const {
  return m_element ? MasterPage(m_document, m_element->master_page(m_document))
                   : MasterPage();
}

PageLayout MasterPage::page_layout() const {
  return m_element ? m_element->page_layout(m_document) : PageLayout();
}

TextStyle LineBreak::style() const {
  return m_element ? m_element->style(m_document) : TextStyle();
}

ParagraphStyle Paragraph::style() const {
  return m_element ? m_element->style(m_document) : ParagraphStyle();
}

TextStyle Paragraph::text_style() const {
  return m_element ? m_element->text_style(m_document) : TextStyle();
}

TextStyle Span::style() const {
  return m_element ? m_element->style(m_document) : TextStyle();
}

std::string Text::content() const {
  return m_element ? m_element->content(m_document) : "";
}

void Text::set_content(const std::string &text) const {
  if (m_element) {
    m_element->set_content(m_document, text);
  }
}

TextStyle Text::style() const {
  return m_element ? m_element->style(m_document) : TextStyle();
}

std::string Link::href() const {
  return m_element ? m_element->href(m_document) : "";
}

std::string Bookmark::name() const {
  return m_element ? m_element->name(m_document) : "";
}

TextStyle ListItem::style() const {
  return m_element ? m_element->style(m_document) : TextStyle();
}

ElementRange Table::columns() const {
  return m_element ? ElementRange(ElementIterator(
                         m_document, m_element->first_column(m_document)))
                   : ElementRange();
}

ElementRange Table::rows() const {
  return m_element ? ElementRange(ElementIterator(
                         m_document, m_element->first_row(m_document)))
                   : ElementRange();
}

TableDimensions Table::dimensions() const {
  return m_element ? m_element->dimensions(m_document) : TableDimensions();
}

TableStyle Table::style() const {
  return m_element ? m_element->style(m_document) : TableStyle();
}

TableColumnStyle TableColumn::style() const {
  return m_element ? m_element->style(m_document) : TableColumnStyle();
}

TableRowStyle TableRow::style() const {
  return m_element ? m_element->style(m_document) : TableRowStyle();
}

TableColumn TableCell::column() const {
  return m_element ? TableColumn(m_document, m_element->column(m_document))
                   : TableColumn();
}

TableRow TableCell::row() const {
  return m_element ? TableRow(m_document, m_element->row(m_document))
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

TableCellStyle TableCell::style() const {
  return m_element ? m_element->style(m_document) : TableCellStyle();
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

GraphicStyle Frame::style() const {
  return m_element ? m_element->style(m_document) : GraphicStyle();
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

GraphicStyle Rect::style() const {
  return m_element ? m_element->style(m_document) : GraphicStyle();
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

GraphicStyle Line::style() const {
  return m_element ? m_element->style(m_document) : GraphicStyle();
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

GraphicStyle Circle::style() const {
  return m_element ? m_element->style(m_document) : GraphicStyle();
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

GraphicStyle CustomShape::style() const {
  return m_element ? m_element->style(m_document) : GraphicStyle();
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
