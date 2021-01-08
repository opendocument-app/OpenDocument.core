#include <abstract/document_elements.h>
#include <odr/document_elements.h>
#include <odr/document_style.h>
#include <odr/file.h>

namespace odr {

Element::Element() = default;

Element::Element(std::shared_ptr<const abstract::Element> impl)
    : m_impl{std::move(impl)} {}

bool Element::operator==(const Element &rhs) const {
  return m_impl == rhs.m_impl;
}

bool Element::operator!=(const Element &rhs) const {
  return m_impl != rhs.m_impl;
}

Element::operator bool() const { return m_impl.operator bool(); }

Element Element::parent() const {
  if (!m_impl)
    return Element();
  return Element(m_impl->parent());
}

Element Element::firstChild() const {
  if (!m_impl)
    return Element();
  return Element(m_impl->firstChild());
}

Element Element::previousSibling() const {
  if (!m_impl)
    return Element();
  return Element(m_impl->previousSibling());
}

Element Element::nextSibling() const {
  if (!m_impl)
    return Element();
  return Element(m_impl->nextSibling());
}

ElementRange Element::children() const { return ElementRange(firstChild()); }

ElementType Element::type() const {
  if (!m_impl)
    return ElementType::NONE;
  return m_impl->type();
}

SlideElement Element::slide() const {
  return SlideElement(std::dynamic_pointer_cast<const abstract::Slide>(m_impl));
}

SheetElement Element::sheet() const {
  return SheetElement(std::dynamic_pointer_cast<const abstract::Sheet>(m_impl));
}

PageElement Element::page() const {
  return PageElement(std::dynamic_pointer_cast<const abstract::Page>(m_impl));
}

TextElement Element::text() const {
  return TextElement(
      std::dynamic_pointer_cast<const abstract::TextElement>(m_impl));
}

Element Element::lineBreak() const {
  if (type() != ElementType::LINE_BREAK)
    return {};
  return *this;
}

Element Element::pageBreak() const {
  if (type() != ElementType::PAGE_BREAK)
    return {};
  return *this;
}

ParagraphElement Element::paragraph() const {
  return ParagraphElement(
      std::dynamic_pointer_cast<const abstract::Paragraph>(m_impl));
}

SpanElement Element::span() const {
  return SpanElement(std::dynamic_pointer_cast<const abstract::Span>(m_impl));
}

LinkElement Element::link() const {
  return LinkElement(std::dynamic_pointer_cast<const abstract::Link>(m_impl));
}

BookmarkElement Element::bookmark() const {
  return BookmarkElement(
      std::dynamic_pointer_cast<const abstract::Bookmark>(m_impl));
}

ListElement Element::list() const {
  return ListElement(std::dynamic_pointer_cast<const abstract::List>(m_impl));
}

ListItemElement Element::listItem() const {
  return ListItemElement(
      std::dynamic_pointer_cast<const abstract::ListItem>(m_impl));
}

TableElement Element::table() const {
  return TableElement(std::dynamic_pointer_cast<const abstract::Table>(m_impl));
}

TableColumnElement Element::tableColumn() const {
  return TableColumnElement(
      std::dynamic_pointer_cast<const abstract::TableColumn>(m_impl));
}

TableRowElement Element::tableRow() const {
  return TableRowElement(
      std::dynamic_pointer_cast<const abstract::TableRow>(m_impl));
}

TableCellElement Element::tableCell() const {
  return TableCellElement(
      std::dynamic_pointer_cast<const abstract::TableCell>(m_impl));
}

FrameElement Element::frame() const {
  return FrameElement(std::dynamic_pointer_cast<const abstract::Frame>(m_impl));
}

ImageElement Element::image() const {
  return ImageElement(std::dynamic_pointer_cast<const abstract::Image>(m_impl));
}

RectElement Element::rect() const {
  return RectElement(std::dynamic_pointer_cast<const abstract::Rect>(m_impl));
}

LineElement Element::line() const {
  return LineElement(std::dynamic_pointer_cast<const abstract::Line>(m_impl));
}

CircleElement Element::circle() const {
  return CircleElement(
      std::dynamic_pointer_cast<const abstract::Circle>(m_impl));
}

template <typename E>
ElementIterator<E>::ElementIterator(E element)
    : m_element{std::move(element)} {}

template <typename E> ElementIterator<E> &ElementIterator<E>::operator++() {
  m_element = m_element.nextSibling();
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

SlideElement::SlideElement(std::shared_ptr<const abstract::Slide> impl)
    : Element(impl), m_impl{std::move(impl)} {}

SlideElement SlideElement::previousSibling() const {
  return Element::previousSibling().slide();
}

SlideElement SlideElement::nextSibling() const {
  return Element::nextSibling().slide();
}

std::string SlideElement::name() const { return m_impl->name(); }

PageStyle SlideElement::pageStyle() const {
  if (!m_impl)
    return PageStyle();
  return PageStyle(m_impl->pageStyle());
}

SheetElement::SheetElement() = default;

SheetElement::SheetElement(std::shared_ptr<const abstract::Sheet> impl)
    : Element(impl), m_impl{std::move(impl)} {}

SheetElement SheetElement::previousSibling() const {
  return Element::previousSibling().sheet();
}

SheetElement SheetElement::nextSibling() const {
  return Element::nextSibling().sheet();
}

std::string SheetElement::name() const { return m_impl->name(); }

TableElement SheetElement::table() const {
  if (!m_impl)
    return TableElement();
  return TableElement(m_impl->table());
}

PageElement::PageElement() = default;

PageElement::PageElement(std::shared_ptr<const abstract::Page> impl)
    : Element(impl), m_impl{std::move(impl)} {}

PageElement PageElement::previousSibling() const {
  return Element::previousSibling().page();
}

PageElement PageElement::nextSibling() const {
  return Element::nextSibling().page();
}

std::string PageElement::name() const {
  if (!m_impl)
    return "";
  return m_impl->name();
}

PageStyle PageElement::pageStyle() const {
  if (!m_impl)
    return PageStyle();
  return PageStyle(m_impl->pageStyle());
}

TextElement::TextElement() = default;

TextElement::TextElement(std::shared_ptr<const abstract::TextElement> impl)
    : Element(impl), m_impl{std::move(impl)} {}

std::string TextElement::string() const { return m_impl->text(); }

ParagraphElement::ParagraphElement() = default;

ParagraphElement::ParagraphElement(
    std::shared_ptr<const abstract::Paragraph> impl)
    : Element(impl), m_impl{std::move(impl)} {}

ParagraphStyle ParagraphElement::paragraphStyle() const {
  if (!m_impl)
    return ParagraphStyle();
  return ParagraphStyle(m_impl->paragraphStyle());
}

TextStyle ParagraphElement::textStyle() const {
  if (!m_impl)
    return TextStyle();
  return TextStyle(m_impl->textStyle());
}

SpanElement::SpanElement() = default;

SpanElement::SpanElement(std::shared_ptr<const abstract::Span> impl)
    : Element(impl), m_impl{std::move(impl)} {}

TextStyle SpanElement::textStyle() const {
  if (!m_impl)
    return TextStyle();
  return TextStyle(m_impl->textStyle());
}

LinkElement::LinkElement() = default;

LinkElement::LinkElement(std::shared_ptr<const abstract::Link> impl)
    : Element(impl), m_impl{std::move(impl)} {}

TextStyle LinkElement::textStyle() const {
  if (!m_impl)
    return TextStyle();
  return TextStyle(m_impl->textStyle());
}

std::string LinkElement::href() const { return m_impl->href(); }

BookmarkElement::BookmarkElement() = default;

BookmarkElement::BookmarkElement(std::shared_ptr<const abstract::Bookmark> impl)
    : Element(impl), m_impl{std::move(impl)} {}

std::string BookmarkElement::name() const {
  if (!m_impl)
    return "";
  return m_impl->name();
}

ListElement::ListElement() = default;

ListElement::ListElement(std::shared_ptr<const abstract::List> impl)
    : Element(impl), m_impl{std::move(impl)} {}

ListItemElement::ListItemElement() = default;

ListItemElement::ListItemElement(std::shared_ptr<const abstract::ListItem> impl)
    : Element(impl), m_impl{std::move(impl)} {}

TableElement::TableElement() = default;

TableElement::TableElement(std::shared_ptr<const abstract::Table> impl)
    : Element(impl), m_impl{std::move(impl)} {}

TableColumnRange TableElement::columns() const {
  if (!m_impl)
    return TableColumnRange();
  return TableColumnRange(TableColumnElement(m_impl->firstColumn()));
}

TableRowRange TableElement::rows() const {
  if (!m_impl)
    return TableRowRange();
  return TableRowRange(TableRowElement(m_impl->firstRow()));
}

TableStyle TableElement::tableStyle() const {
  if (!m_impl)
    return TableStyle();
  return TableStyle(m_impl->tableStyle());
}

TableColumnElement::TableColumnElement() = default;

TableColumnElement::TableColumnElement(
    std::shared_ptr<const abstract::TableColumn> impl)
    : Element(impl), m_impl{std::move(impl)} {}

TableColumnElement TableColumnElement::previousSibling() const {
  return Element::previousSibling().tableColumn();
}

TableColumnElement TableColumnElement::nextSibling() const {
  return Element::nextSibling().tableColumn();
}

TableColumnStyle TableColumnElement::tableColumnStyle() const {
  if (!m_impl)
    return TableColumnStyle();
  return TableColumnStyle(m_impl->tableColumnStyle());
}

TableRowElement::TableRowElement() = default;

TableRowElement::TableRowElement(std::shared_ptr<const abstract::TableRow> impl)
    : Element(impl), m_impl{std::move(impl)} {}

TableCellElement TableRowElement::firstChild() const {
  if (!m_impl)
    return TableCellElement();
  return Element(m_impl->firstChild()).tableCell();
}

TableRowElement TableRowElement::previousSibling() const {
  return Element::previousSibling().tableRow();
}

TableRowElement TableRowElement::nextSibling() const {
  return Element::nextSibling().tableRow();
}

TableCellRange TableRowElement::cells() const {
  return TableCellRange(firstChild());
}

TableCellElement::TableCellElement() = default;

TableCellElement::TableCellElement(
    std::shared_ptr<const abstract::TableCell> impl)
    : Element(impl), m_impl{std::move(impl)} {}

TableCellElement TableCellElement::previousSibling() const {
  return Element::previousSibling().tableCell();
}

TableCellElement TableCellElement::nextSibling() const {
  return Element::nextSibling().tableCell();
}

std::uint32_t TableCellElement::rowSpan() const {
  if (!m_impl)
    return 0;
  return m_impl->rowSpan();
}

std::uint32_t TableCellElement::columnSpan() const {
  if (!m_impl)
    return 0;
  return m_impl->columnSpan();
}

TableCellStyle TableCellElement::tableCellStyle() const {
  if (!m_impl)
    return TableCellStyle();
  return TableCellStyle(m_impl->tableCellStyle());
}

FrameElement::FrameElement() = default;

FrameElement::FrameElement(std::shared_ptr<const abstract::Frame> impl)
    : Element(impl), m_impl{std::move(impl)} {}

Property FrameElement::anchorType() const {
  if (!m_impl)
    return Property();
  return Property(m_impl->anchorType());
}

Property FrameElement::width() const {
  if (!m_impl)
    return Property();
  return Property(m_impl->width());
}

Property FrameElement::height() const {
  if (!m_impl)
    return Property();
  return Property(m_impl->height());
}

Property FrameElement::zIndex() const {
  if (!m_impl)
    return Property();
  return Property(m_impl->zIndex());
}

ImageElement::ImageElement() = default;

ImageElement::ImageElement(std::shared_ptr<const abstract::Image> impl)
    : Element(impl), m_impl{std::move(impl)} {}

bool ImageElement::internal() const {
  if (!m_impl)
    return false;
  return m_impl->internal();
}

std::string ImageElement::href() const {
  if (!m_impl)
    return "";
  return m_impl->href();
}

ImageFile ImageElement::imageFile() const {
  if (!m_impl)
    return ImageFile({}); // TODO
  return m_impl->imageFile();
}

RectElement::RectElement() = default;

RectElement::RectElement(std::shared_ptr<const abstract::Rect> impl)
    : Element(impl), m_impl{std::move(impl)} {}

std::string RectElement::x() const {
  if (!m_impl)
    return "";
  return m_impl->x();
}

std::string RectElement::y() const {
  if (!m_impl)
    return "";
  return m_impl->y();
}

std::string RectElement::width() const {
  if (!m_impl)
    return "";
  return m_impl->width();
}

std::string RectElement::height() const {
  if (!m_impl)
    return "";
  return m_impl->height();
}

DrawingStyle RectElement::drawingStyle() const {
  if (!m_impl)
    return DrawingStyle();
  return DrawingStyle(m_impl->drawingStyle());
}

LineElement::LineElement() = default;

LineElement::LineElement(std::shared_ptr<const abstract::Line> impl)
    : Element(impl), m_impl{std::move(impl)} {}

std::string LineElement::x1() const {
  if (!m_impl)
    return "";
  return m_impl->x1();
}

std::string LineElement::y1() const {
  if (!m_impl)
    return "";
  return m_impl->y1();
}

std::string LineElement::x2() const {
  if (!m_impl)
    return "";
  return m_impl->x2();
}

std::string LineElement::y2() const {
  if (!m_impl)
    return "";
  return m_impl->y2();
}

DrawingStyle LineElement::drawingStyle() const {
  if (!m_impl)
    return DrawingStyle();
  return DrawingStyle(m_impl->drawingStyle());
}

CircleElement::CircleElement() = default;

CircleElement::CircleElement(std::shared_ptr<const abstract::Circle> impl)
    : Element(impl), m_impl{std::move(impl)} {}

std::string CircleElement::x() const {
  if (!m_impl)
    return "";
  return m_impl->x();
}

std::string CircleElement::y() const {
  if (!m_impl)
    return "";
  return m_impl->y();
}

std::string CircleElement::width() const {
  if (!m_impl)
    return "";
  return m_impl->width();
}

std::string CircleElement::height() const {
  if (!m_impl)
    return "";
  return m_impl->height();
}

DrawingStyle CircleElement::drawingStyle() const {
  if (!m_impl)
    return DrawingStyle();
  return DrawingStyle(m_impl->drawingStyle());
}

} // namespace odr
