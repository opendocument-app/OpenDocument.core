#include <internal/abstract/document_elements.h>
#include <odr/document_elements.h>
#include <odr/document_style.h>
#include <odr/element_type.h>
#include <odr/file.h>
#include <odr/property.h>
#include <odr/table_dimensions.h>

namespace odr {

Element::Element() = default;

Element::Element(std::shared_ptr<const internal::abstract::Element> impl)
    : m_impl{std::move(impl)} {}

bool Element::operator==(const Element &rhs) const {
  return m_impl == rhs.m_impl;
}

bool Element::operator!=(const Element &rhs) const {
  return m_impl != rhs.m_impl;
}

Element::operator bool() const { return m_impl.operator bool(); }

Element Element::parent() const {
  if (!m_impl) {
    return Element();
  }
  return Element(m_impl->parent());
}

Element Element::first_child() const {
  if (!m_impl) {
    return Element();
  }
  return Element(m_impl->first_child());
}

Element Element::previous_sibling() const {
  if (!m_impl) {
    return Element();
  }
  return Element(m_impl->previous_sibling());
}

Element Element::next_sibling() const {
  if (!m_impl) {
    return Element();
  }
  return Element(m_impl->next_sibling());
}

ElementRange Element::children() const { return ElementRange(first_child()); }

ElementType Element::type() const {
  if (!m_impl) {
    return ElementType::NONE;
  }
  return m_impl->type();
}

SlideElement Element::slide() const {
  return SlideElement(
      std::dynamic_pointer_cast<const internal::abstract::Slide>(m_impl));
}

SheetElement Element::sheet() const {
  return SheetElement(
      std::dynamic_pointer_cast<const internal::abstract::Sheet>(m_impl));
}

PageElement Element::page() const {
  return PageElement(
      std::dynamic_pointer_cast<const internal::abstract::Page>(m_impl));
}

TextElement Element::text() const {
  return TextElement(
      std::dynamic_pointer_cast<const internal::abstract::TextElement>(m_impl));
}

Element Element::line_break() const {
  if (type() != ElementType::LINE_BREAK) {
    return {};
  }
  return *this;
}

Element Element::page_break() const {
  if (type() != ElementType::PAGE_BREAK) {
    return {};
  }
  return *this;
}

ParagraphElement Element::paragraph() const {
  return ParagraphElement(
      std::dynamic_pointer_cast<const internal::abstract::Paragraph>(m_impl));
}

SpanElement Element::span() const {
  return SpanElement(
      std::dynamic_pointer_cast<const internal::abstract::Span>(m_impl));
}

LinkElement Element::link() const {
  return LinkElement(
      std::dynamic_pointer_cast<const internal::abstract::Link>(m_impl));
}

BookmarkElement Element::bookmark() const {
  return BookmarkElement(
      std::dynamic_pointer_cast<const internal::abstract::Bookmark>(m_impl));
}

ListElement Element::list() const {
  return ListElement(
      std::dynamic_pointer_cast<const internal::abstract::List>(m_impl));
}

ListItemElement Element::list_item() const {
  return ListItemElement(
      std::dynamic_pointer_cast<const internal::abstract::ListItem>(m_impl));
}

TableElement Element::table() const {
  return TableElement(
      std::dynamic_pointer_cast<const internal::abstract::Table>(m_impl));
}

TableColumnElement Element::table_column() const {
  return TableColumnElement(
      std::dynamic_pointer_cast<const internal::abstract::TableColumn>(m_impl));
}

TableRowElement Element::table_row() const {
  return TableRowElement(
      std::dynamic_pointer_cast<const internal::abstract::TableRow>(m_impl));
}

TableCellElement Element::table_cell() const {
  return TableCellElement(
      std::dynamic_pointer_cast<const internal::abstract::TableCell>(m_impl));
}

FrameElement Element::frame() const {
  return FrameElement(
      std::dynamic_pointer_cast<const internal::abstract::Frame>(m_impl));
}

ImageElement Element::image() const {
  return ImageElement(
      std::dynamic_pointer_cast<const internal::abstract::Image>(m_impl));
}

RectElement Element::rect() const {
  return RectElement(
      std::dynamic_pointer_cast<const internal::abstract::Rect>(m_impl));
}

LineElement Element::line() const {
  return LineElement(
      std::dynamic_pointer_cast<const internal::abstract::Line>(m_impl));
}

CircleElement Element::circle() const {
  return CircleElement(
      std::dynamic_pointer_cast<const internal::abstract::Circle>(m_impl));
}

template <typename E>
ElementIterator<E>::ElementIterator(E element)
    : m_element{std::move(element)} {}

template <typename E> ElementIterator<E> &ElementIterator<E>::operator++() {
  m_element = m_element.next_sibling();
  return *this;
}

template <typename E> ElementIterator<E> ElementIterator<E>::operator++(int) & {
  ElementIterator<E> result = *this;
  operator++();
  return result;
}

template <typename E> E &ElementIterator<E>::operator*() { return m_element; }

template <typename E> E *ElementIterator<E>::operator->() { return &m_element; }

template <typename E>
bool ElementIterator<E>::operator==(const ElementIterator<E> &rhs) const {
  return m_element == rhs.m_element;
}

template <typename E>
bool ElementIterator<E>::operator!=(const ElementIterator<E> &rhs) const {
  return m_element != rhs.m_element;
}

template class ElementIterator<Element>;
template class ElementIterator<SlideElement>;
template class ElementIterator<SheetElement>;
template class ElementIterator<PageElement>;
template class ElementIterator<TableColumnElement>;
template class ElementIterator<TableRowElement>;
template class ElementIterator<TableCellElement>;

template <typename E>
ElementRangeTemplate<E>::ElementRangeTemplate()
    : ElementRangeTemplate({}, {}) {}

template <typename E>
ElementRangeTemplate<E>::ElementRangeTemplate(E begin)
    : ElementRangeTemplate(std::move(begin), {}) {}

template <typename E>
ElementRangeTemplate<E>::ElementRangeTemplate(E begin, E end)
    : m_begin{std::move(begin)}, m_end{std::move(end)} {}

template <typename E>
ElementIterator<E> ElementRangeTemplate<E>::begin() const {
  return ElementIterator<E>(m_begin);
}

template <typename E> ElementIterator<E> ElementRangeTemplate<E>::end() const {
  return ElementIterator<E>(m_end);
}

template <typename E> E ElementRangeTemplate<E>::front() const {
  return m_begin;
}

template class ElementRangeTemplate<Element>;
template class ElementRangeTemplate<SlideElement>;
template class ElementRangeTemplate<SheetElement>;
template class ElementRangeTemplate<PageElement>;
template class ElementRangeTemplate<TableColumnElement>;
template class ElementRangeTemplate<TableRowElement>;
template class ElementRangeTemplate<TableCellElement>;

SlideElement::SlideElement() = default;

SlideElement::SlideElement(
    std::shared_ptr<const internal::abstract::Slide> impl)
    : Element(impl), m_impl{std::move(impl)} {}

SlideElement SlideElement::previous_sibling() const {
  return Element::previous_sibling().slide();
}

SlideElement SlideElement::next_sibling() const {
  return Element::next_sibling().slide();
}

std::string SlideElement::name() const { return m_impl->name(); }

PageStyle SlideElement::page_style() const {
  if (!m_impl) {
    return PageStyle();
  }
  return PageStyle(m_impl->page_style());
}

SheetElement::SheetElement() = default;

SheetElement::SheetElement(
    std::shared_ptr<const internal::abstract::Sheet> impl)
    : Element(impl), m_impl{std::move(impl)} {}

SheetElement SheetElement::previous_sibling() const {
  return Element::previous_sibling().sheet();
}

SheetElement SheetElement::next_sibling() const {
  return Element::next_sibling().sheet();
}

std::string SheetElement::name() const { return m_impl->name(); }

TableElement SheetElement::table() const {
  if (!m_impl) {
    return TableElement();
  }
  return TableElement(m_impl->table());
}

PageElement::PageElement() = default;

PageElement::PageElement(std::shared_ptr<const internal::abstract::Page> impl)
    : Element(impl), m_impl{std::move(impl)} {}

PageElement PageElement::previous_sibling() const {
  return Element::previous_sibling().page();
}

PageElement PageElement::next_sibling() const {
  return Element::next_sibling().page();
}

std::string PageElement::name() const {
  if (!m_impl) {
    return "";
  }
  return m_impl->name();
}

PageStyle PageElement::page_style() const {
  if (!m_impl) {
    return PageStyle();
  }
  return PageStyle(m_impl->page_style());
}

TextElement::TextElement() = default;

TextElement::TextElement(
    std::shared_ptr<const internal::abstract::TextElement> impl)
    : Element(impl), m_impl{std::move(impl)} {}

std::string TextElement::string() const { return m_impl->text(); }

ParagraphElement::ParagraphElement() = default;

ParagraphElement::ParagraphElement(
    std::shared_ptr<const internal::abstract::Paragraph> impl)
    : Element(impl), m_impl{std::move(impl)} {}

ParagraphStyle ParagraphElement::paragraph_style() const {
  if (!m_impl) {
    return ParagraphStyle();
  }
  return ParagraphStyle(m_impl->paragraph_style());
}

TextStyle ParagraphElement::text_style() const {
  if (!m_impl) {
    return TextStyle();
  }
  return TextStyle(m_impl->text_style());
}

SpanElement::SpanElement() = default;

SpanElement::SpanElement(std::shared_ptr<const internal::abstract::Span> impl)
    : Element(impl), m_impl{std::move(impl)} {}

TextStyle SpanElement::text_style() const {
  if (!m_impl) {
    return TextStyle();
  }
  return TextStyle(m_impl->text_style());
}

LinkElement::LinkElement() = default;

LinkElement::LinkElement(std::shared_ptr<const internal::abstract::Link> impl)
    : Element(impl), m_impl{std::move(impl)} {}

TextStyle LinkElement::text_style() const {
  if (!m_impl) {
    return TextStyle();
  }
  return TextStyle(m_impl->text_style());
}

std::string LinkElement::href() const { return m_impl->href(); }

BookmarkElement::BookmarkElement() = default;

BookmarkElement::BookmarkElement(
    std::shared_ptr<const internal::abstract::Bookmark> impl)
    : Element(impl), m_impl{std::move(impl)} {}

std::string BookmarkElement::name() const {
  if (!m_impl) {
    return "";
  }
  return m_impl->name();
}

ListElement::ListElement() = default;

ListElement::ListElement(std::shared_ptr<const internal::abstract::List> impl)
    : Element(impl), m_impl{std::move(impl)} {}

ListItemElement::ListItemElement() = default;

ListItemElement::ListItemElement(
    std::shared_ptr<const internal::abstract::ListItem> impl)
    : Element(impl), m_impl{std::move(impl)} {}

TableElement::TableElement() = default;

TableElement::TableElement(
    std::shared_ptr<const internal::abstract::Table> impl)
    : Element(impl), m_impl{std::move(impl)} {}

TableDimensions TableElement::dimensions() const {
  if (!m_impl) {
    return TableDimensions(); // TODO
  }
  return m_impl->dimensions();
}

TableColumnRange TableElement::columns() const {
  if (!m_impl) {
    return TableColumnRange();
  }
  return TableColumnRange(TableColumnElement(m_impl->first_column()));
}

TableRowRange TableElement::rows() const {
  if (!m_impl) {
    return TableRowRange();
  }
  return TableRowRange(TableRowElement(m_impl->first_row()));
}

TableStyle TableElement::table_style() const {
  if (!m_impl) {
    return TableStyle();
  }
  return TableStyle(m_impl->table_style());
}

TableColumnElement::TableColumnElement() = default;

TableColumnElement::TableColumnElement(
    std::shared_ptr<const internal::abstract::TableColumn> impl)
    : Element(impl), m_impl{std::move(impl)} {}

TableColumnElement TableColumnElement::previous_sibling() const {
  return Element::previous_sibling().table_column();
}

TableColumnElement TableColumnElement::next_sibling() const {
  return Element::next_sibling().table_column();
}

TableColumnStyle TableColumnElement::table_column_style() const {
  if (!m_impl) {
    return TableColumnStyle();
  }
  return TableColumnStyle(m_impl->table_column_style());
}

TableRowElement::TableRowElement() = default;

TableRowElement::TableRowElement(
    std::shared_ptr<const internal::abstract::TableRow> impl)
    : Element(impl), m_impl{std::move(impl)} {}

TableCellElement TableRowElement::first_child() const {
  if (!m_impl) {
    return TableCellElement();
  }
  return Element(m_impl->first_child()).table_cell();
}

TableRowElement TableRowElement::previous_sibling() const {
  return Element::previous_sibling().table_row();
}

TableRowElement TableRowElement::next_sibling() const {
  return Element::next_sibling().table_row();
}

TableCellRange TableRowElement::cells() const {
  return TableCellRange(first_child());
}

TableCellElement::TableCellElement() = default;

TableCellElement::TableCellElement(
    std::shared_ptr<const internal::abstract::TableCell> impl)
    : Element(impl), m_impl{std::move(impl)} {}

TableCellElement TableCellElement::previous_sibling() const {
  return Element::previous_sibling().table_cell();
}

TableCellElement TableCellElement::next_sibling() const {
  return Element::next_sibling().table_cell();
}

std::uint32_t TableCellElement::row_span() const {
  if (!m_impl) {
    return 0;
  }
  return m_impl->row_span();
}

std::uint32_t TableCellElement::column_span() const {
  if (!m_impl) {
    return 0;
  }
  return m_impl->column_span();
}

TableCellStyle TableCellElement::table_cell_style() const {
  if (!m_impl) {
    return TableCellStyle();
  }
  return TableCellStyle(m_impl->table_cell_style());
}

FrameElement::FrameElement() = default;

FrameElement::FrameElement(
    std::shared_ptr<const internal::abstract::Frame> impl)
    : Element(impl), m_impl{std::move(impl)} {}

Property FrameElement::anchor_type() const {
  if (!m_impl) {
    return Property();
  }
  return Property(m_impl->anchor_type());
}

Property FrameElement::width() const {
  if (!m_impl) {
    return Property();
  }
  return Property(m_impl->width());
}

Property FrameElement::height() const {
  if (!m_impl) {
    return Property();
  }
  return Property(m_impl->height());
}

Property FrameElement::z_index() const {
  if (!m_impl) {
    return Property();
  }
  return Property(m_impl->z_index());
}

ImageElement::ImageElement() = default;

ImageElement::ImageElement(
    std::shared_ptr<const internal::abstract::Image> impl)
    : Element(impl), m_impl{std::move(impl)} {}

bool ImageElement::internal() const {
  if (!m_impl) {
    return false;
  }
  return m_impl->internal();
}

std::string ImageElement::href() const {
  if (!m_impl) {
    return "";
  }
  return m_impl->href();
}

File ImageElement::image_file() const {
  if (!m_impl) {
    // TODO there is no "empty" file
    return File(std::shared_ptr<internal::abstract::File>());
  }
  return File(m_impl->image_file());
}

RectElement::RectElement() = default;

RectElement::RectElement(std::shared_ptr<const internal::abstract::Rect> impl)
    : Element(impl), m_impl{std::move(impl)} {}

std::string RectElement::x() const {
  if (!m_impl) {
    return "";
  }
  return m_impl->x();
}

std::string RectElement::y() const {
  if (!m_impl) {
    return "";
  }
  return m_impl->y();
}

std::string RectElement::width() const {
  if (!m_impl) {
    return "";
  }
  return m_impl->width();
}

std::string RectElement::height() const {
  if (!m_impl) {
    return "";
  }
  return m_impl->height();
}

DrawingStyle RectElement::drawing_style() const {
  if (!m_impl) {
    return DrawingStyle();
  }
  return DrawingStyle(m_impl->drawing_style());
}

LineElement::LineElement() = default;

LineElement::LineElement(std::shared_ptr<const internal::abstract::Line> impl)
    : Element(impl), m_impl{std::move(impl)} {}

std::string LineElement::x1() const {
  if (!m_impl) {
    return "";
  }
  return m_impl->x1();
}

std::string LineElement::y1() const {
  if (!m_impl) {
    return "";
  }
  return m_impl->y1();
}

std::string LineElement::x2() const {
  if (!m_impl) {
    return "";
  }
  return m_impl->x2();
}

std::string LineElement::y2() const {
  if (!m_impl) {
    return "";
  }
  return m_impl->y2();
}

DrawingStyle LineElement::drawing_style() const {
  if (!m_impl) {
    return DrawingStyle();
  }
  return DrawingStyle(m_impl->drawing_style());
}

CircleElement::CircleElement() = default;

CircleElement::CircleElement(
    std::shared_ptr<const internal::abstract::Circle> impl)
    : Element(impl), m_impl{std::move(impl)} {}

std::string CircleElement::x() const {
  if (!m_impl) {
    return "";
  }
  return m_impl->x();
}

std::string CircleElement::y() const {
  if (!m_impl) {
    return "";
  }
  return m_impl->y();
}

std::string CircleElement::width() const {
  if (!m_impl) {
    return "";
  }
  return m_impl->width();
}

std::string CircleElement::height() const {
  if (!m_impl) {
    return "";
  }
  return m_impl->height();
}

DrawingStyle CircleElement::drawing_style() const {
  if (!m_impl) {
    return DrawingStyle();
  }
  return DrawingStyle(m_impl->drawing_style());
}

} // namespace odr
