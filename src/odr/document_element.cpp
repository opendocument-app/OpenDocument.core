#include <odr/document_element.hpp>

#include <odr/file.hpp>
#include <odr/style.hpp>

#include <odr/internal/abstract/document_element.hpp>

namespace odr {

Element::Element() = default;

Element::Element(
    const internal::abstract::Document *document,
    std::pair<internal::abstract::Element *, ElementIdentifier> element)
    : m_document{document}, m_element{element} {}

bool Element::operator==(const Element &rhs) const {
  return m_element == rhs.m_element;
}

bool Element::operator!=(const Element &rhs) const {
  return m_element != rhs.m_element;
}

Element::operator bool() const { return exists_(); }

bool Element::exists_() const { return m_element.first != nullptr; }

internal::abstract::Element *Element::element_() const {
  return m_element.first;
}

ElementIdentifier Element::id_() const { return m_element.second; }

ElementType Element::type() const {
  return exists_() ? element_()->type(m_document, id_()) : ElementType::none;
}

Element Element::parent() const {
  return exists_() ? Element(m_document, element_()->parent(m_document, id_()))
                   : Element();
}

Element Element::first_child() const {
  return exists_()
             ? Element(m_document, element_()->first_child(m_document, id_()))
             : Element();
}

Element Element::previous_sibling() const {
  return exists_() ? Element(m_document,
                             element_()->previous_sibling(m_document, id_()))
                   : Element();
}

Element Element::next_sibling() const {
  return exists_()
             ? Element(m_document, element_()->next_sibling(m_document, id_()))
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

ElementRange Element::children() const {
  return {exists_() ? ElementIterator(m_document, element_()->first_child(
                                                      m_document, id_()))
                    : ElementIterator(),
          ElementIterator()};
}

ElementIterator::ElementIterator() = default;

ElementIterator::ElementIterator(const internal::abstract::Document *document,
                                 InternalElement element)
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
  if (exists_()) {
    m_element = element_()->next_sibling(m_document, id_());
  }
  return *this;
}

ElementIterator ElementIterator::operator++(int) {
  if (!exists_()) {
    return {};
  }
  return {m_document, element_()->next_sibling(m_document, id_())};
}

bool ElementIterator::exists_() const { return m_element.first != nullptr; }

internal::abstract::Element *ElementIterator::element_() const {
  return m_element.first;
}

ElementIdentifier ElementIterator::id_() const { return m_element.second; }

ElementRange::ElementRange() = default;

ElementRange::ElementRange(ElementIterator begin) : m_begin{std::move(begin)} {}

ElementRange::ElementRange(ElementIterator begin, ElementIterator end)
    : m_begin{std::move(begin)}, m_end{std::move(end)} {}

ElementIterator ElementRange::begin() const { return m_begin; }

ElementIterator ElementRange::end() const { return m_end; }

PageLayout TextRoot::page_layout() const {
  return exists_() ? element_()->page_layout(m_document, id_()) : PageLayout();
}

MasterPage TextRoot::first_master_page() const {
  return exists_()
             ? MasterPage(m_document,
                          element_()->first_master_page(m_document, id_()))
             : MasterPage();
}

std::string Slide::name() const {
  return exists_() ? element_()->name(m_document, id_()) : "";
}

PageLayout Slide::page_layout() const {
  return exists_() ? element_()->page_layout(m_document, id_()) : PageLayout();
}

MasterPage Slide::master_page() const {
  return exists_() ? MasterPage(m_document,
                                element_()->master_page(m_document, id_()))
                   : MasterPage();
}

std::string Sheet::name() const {
  return exists_() ? element_()->name(m_document, id_()) : "";
}

TableDimensions Sheet::dimensions() const {
  return exists_() ? element_()->dimensions(m_document, id_())
                   : TableDimensions();
}

TableDimensions Sheet::content(std::optional<TableDimensions> range) const {
  return exists_() ? element_()->content(m_document, id_(), range)
                   : TableDimensions();
}

TableColumn Sheet::column(std::uint32_t column) const {
  return exists_() ? TableColumn(m_document,
                                 {element_()->column(m_document, id_(), column),
                                  0}) // TODO
                   : TableColumn();
}

TableRow Sheet::row(std::uint32_t row) const {
  return exists_()
             ? TableRow(m_document,
                        {element_()->row(m_document, id_(), row), 0}) // TODO
             : TableRow();
}

TableCell Sheet::cell(std::uint32_t column, std::uint32_t row) const {
  return exists_()
             ? TableCell(m_document,
                         {element_()->cell(m_document, id_(), column, row),
                          0}) // TODO
             : TableCell();
}

ElementRange Sheet::shapes() const {
  return exists_()
             ? ElementRange(ElementIterator(
                   m_document,
                   {element_()->first_shape(m_document, id_()), 0})) // TODO
             : ElementRange();
}

TableStyle Sheet::style() const {
  return exists_() ? element_()->style(m_document, id_()) : TableStyle();
}

std::string Page::name() const {
  return exists_() ? element_()->name(m_document, id_()) : "";
}

PageLayout Page::page_layout() const {
  return exists_() ? element_()->page_layout(m_document, id_()) : PageLayout();
}

MasterPage Page::master_page() const {
  return exists_() ? MasterPage(m_document,
                                element_()->master_page(m_document, id_()))
                   : MasterPage();
}

PageLayout MasterPage::page_layout() const {
  return exists_() ? element_()->page_layout(m_document, id_()) : PageLayout();
}

TextStyle LineBreak::style() const {
  return exists_() ? element_()->style(m_document, id_()) : TextStyle();
}

ParagraphStyle Paragraph::style() const {
  return exists_() ? element_()->style(m_document, id_()) : ParagraphStyle();
}

TextStyle Paragraph::text_style() const {
  return exists_() ? element_()->text_style(m_document, id_()) : TextStyle();
}

TextStyle Span::style() const {
  return exists_() ? element_()->style(m_document, id_()) : TextStyle();
}

std::string Text::content() const {
  return exists_() ? element_()->content(m_document, id_()) : "";
}

void Text::set_content(const std::string &text) const {
  if (exists_()) {
    element_()->set_content(m_document, id_(), text);
  }
}

TextStyle Text::style() const {
  return exists_() ? element_()->style(m_document, id_()) : TextStyle();
}

std::string Link::href() const {
  return exists_() ? element_()->href(m_document, id_()) : "";
}

std::string Bookmark::name() const {
  return exists_() ? element_()->name(m_document, id_()) : "";
}

TextStyle ListItem::style() const {
  return exists_() ? element_()->style(m_document, id_()) : TextStyle();
}

ElementRange Table::columns() const {
  return exists_()
             ? ElementRange(ElementIterator(
                   m_document, element_()->first_column(m_document, id_())))
             : ElementRange();
}

ElementRange Table::rows() const {
  return exists_() ? ElementRange(ElementIterator(
                         m_document, element_()->first_row(m_document, id_())))
                   : ElementRange();
}

TableDimensions Table::dimensions() const {
  return exists_() ? element_()->dimensions(m_document, id_())
                   : TableDimensions();
}

TableStyle Table::style() const {
  return exists_() ? element_()->style(m_document, id_()) : TableStyle();
}

TableColumnStyle TableColumn::style() const {
  return exists_() ? element_()->style(m_document, id_()) : TableColumnStyle();
}

TableRowStyle TableRow::style() const {
  return exists_() ? element_()->style(m_document, id_()) : TableRowStyle();
}

bool TableCell::covered() const {
  return exists_() && element_()->covered(m_document, id_());
}

TableDimensions TableCell::span() const {
  return exists_() ? element_()->span(m_document, id_()) : TableDimensions();
}

ValueType TableCell::value_type() const {
  return exists_() ? element_()->value_type(m_document, id_())
                   : ValueType::string;
}

TableCellStyle TableCell::style() const {
  return exists_() ? element_()->style(m_document, id_()) : TableCellStyle();
}

AnchorType Frame::anchor_type() const {
  return exists_() ? element_()->anchor_type(m_document, id_())
                   : AnchorType::as_char; // TODO default?
}

std::optional<std::string> Frame::x() const {
  return exists_() ? element_()->x(m_document, id_())
                   : std::optional<std::string>();
}

std::optional<std::string> Frame::y() const {
  return exists_() ? element_()->y(m_document, id_())
                   : std::optional<std::string>();
}

std::optional<std::string> Frame::width() const {
  return exists_() ? element_()->width(m_document, id_())
                   : std::optional<std::string>();
}

std::optional<std::string> Frame::height() const {
  return exists_() ? element_()->height(m_document, id_())
                   : std::optional<std::string>();
}

std::optional<std::string> Frame::z_index() const {
  return exists_() ? element_()->z_index(m_document, id_())
                   : std::optional<std::string>();
}

GraphicStyle Frame::style() const {
  return exists_() ? element_()->style(m_document, id_()) : GraphicStyle();
}

std::string Rect::x() const {
  return exists_() ? element_()->x(m_document, id_()) : "";
}

std::string Rect::y() const {
  return exists_() ? element_()->y(m_document, id_()) : "";
}

std::string Rect::width() const {
  return exists_() ? element_()->width(m_document, id_()) : "";
}

std::string Rect::height() const {
  return exists_() ? element_()->height(m_document, id_()) : "";
}

GraphicStyle Rect::style() const {
  return exists_() ? element_()->style(m_document, id_()) : GraphicStyle();
}

std::string Line::x1() const {
  return exists_() ? element_()->x1(m_document, id_()) : "";
}

std::string Line::y1() const {
  return exists_() ? element_()->y1(m_document, id_()) : "";
}

std::string Line::x2() const {
  return exists_() ? element_()->x2(m_document, id_()) : "";
}

std::string Line::y2() const {
  return exists_() ? element_()->y2(m_document, id_()) : "";
}

GraphicStyle Line::style() const {
  return exists_() ? element_()->style(m_document, id_()) : GraphicStyle();
}

std::string Circle::x() const {
  return exists_() ? element_()->x(m_document, id_()) : "";
}

std::string Circle::y() const {
  return exists_() ? element_()->y(m_document, id_()) : "";
}

std::string Circle::width() const {
  return exists_() ? element_()->width(m_document, id_()) : "";
}

std::string Circle::height() const {
  return exists_() ? element_()->height(m_document, id_()) : "";
}

GraphicStyle Circle::style() const {
  return exists_() ? element_()->style(m_document, id_()) : GraphicStyle();
}

std::optional<std::string> CustomShape::x() const {
  return exists_() ? element_()->x(m_document, id_()) : "";
}

std::optional<std::string> CustomShape::y() const {
  return exists_() ? element_()->y(m_document, id_()) : "";
}

std::string CustomShape::width() const {
  return exists_() ? element_()->width(m_document, id_()) : "";
}

std::string CustomShape::height() const {
  return exists_() ? element_()->height(m_document, id_()) : "";
}

GraphicStyle CustomShape::style() const {
  return exists_() ? element_()->style(m_document, id_()) : GraphicStyle();
}

bool Image::internal() const {
  return exists_() && element_()->internal(m_document, id_());
}

std::optional<File> Image::file() const {
  return exists_() ? element_()->file(m_document, id_())
                   : std::optional<File>();
}

std::string Image::href() const {
  return exists_() ? element_()->href(m_document, id_()) : "";
}

} // namespace odr
