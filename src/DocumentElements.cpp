#include <common/DocumentElements.h>
#include <odr/DocumentElements.h>

namespace odr {

FontProperties::operator bool() const {
  return font || size || weight || style || color;
}

RectangularProperties::operator bool() const {
  return top || bottom || left || right;
}

Element::Element() = default;

Element::Element(std::shared_ptr<const common::Element> impl)
    : m_impl{std::move(impl)} {}

bool Element::operator==(const Element &rhs) const {
  return m_impl == rhs.m_impl;
}

bool Element::operator!=(const Element &rhs) const {
  return m_impl != rhs.m_impl;
}

Element::operator bool() const { return m_impl.operator bool(); }

Element Element::parent() const { return Element(m_impl->parent()); }

Element Element::firstChild() const { return Element(m_impl->firstChild()); }

Element Element::previousSibling() const {
  return Element(m_impl->previousSibling());
}

Element Element::nextSibling() const { return Element(m_impl->nextSibling()); }

ElementRange Element::children() const { return ElementRange(firstChild()); }

ElementType Element::type() const { return m_impl->type(); }

Element Element::unknown() const {
  if (type() != ElementType::UNKNOWN)
    return {};
  return *this;
}

TextElement Element::text() const {
  return TextElement(
      std::dynamic_pointer_cast<const common::TextElement>(m_impl));
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
      std::dynamic_pointer_cast<const common::Paragraph>(m_impl));
}

SpanElement Element::span() const {
  return SpanElement(std::dynamic_pointer_cast<const common::Span>(m_impl));
}

LinkElement Element::link() const {
  return LinkElement(std::dynamic_pointer_cast<const common::Link>(m_impl));
}

BookmarkElement Element::bookmark() const {
  return BookmarkElement(
      std::dynamic_pointer_cast<const common::Bookmark>(m_impl));
}

ImageElement Element::image() const {
  return ImageElement(std::dynamic_pointer_cast<const common::Image>(m_impl));
}

ListElement Element::list() const {
  return ListElement(std::dynamic_pointer_cast<const common::List>(m_impl));
}

ListItemElement Element::listItem() const {
  return ListItemElement(
      std::dynamic_pointer_cast<const common::ListItem>(m_impl));
}

TableElement Element::table() const {
  return TableElement(std::dynamic_pointer_cast<const common::Table>(m_impl));
}

TableColumnElement Element::tableColumn() const {
  return TableColumnElement(
      std::dynamic_pointer_cast<const common::TableColumn>(m_impl));
}

TableRowElement Element::tableRow() const {
  return TableRowElement(
      std::dynamic_pointer_cast<const common::TableRow>(m_impl));
}

TableCellElement Element::tableCell() const {
  return TableCellElement(
      std::dynamic_pointer_cast<const common::TableCell>(m_impl));
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
template class ElementIterator<TableColumnElement>;
template class ElementIterator<TableRowElement>;
// template class ElementIterator<TableCellElement>;

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
template class ElementRangeTemplate<TableColumnElement>;
template class ElementRangeTemplate<TableRowElement>;
template class ElementRangeTemplate<TableCellElement>;

TextElement::TextElement() = default;

TextElement::TextElement(std::shared_ptr<const common::TextElement> impl)
    : Element(impl), m_impl{std::move(impl)} {}

std::string TextElement::string() const { return m_impl->text(); }

ParagraphElement::ParagraphElement() = default;

ParagraphElement::ParagraphElement(
    std::shared_ptr<const common::Paragraph> impl)
    : Element(impl), m_impl{std::move(impl)} {}

ParagraphProperties ParagraphElement::paragraphProperties() const {
  return m_impl->paragraphProperties();
}

TextProperties ParagraphElement::textProperties() const {
  return m_impl->textProperties();
}

SpanElement::SpanElement() = default;

SpanElement::SpanElement(std::shared_ptr<const common::Span> impl)
    : Element(impl), m_impl{std::move(impl)} {}

TextProperties SpanElement::textProperties() const {
  return m_impl->textProperties();
}

LinkElement::LinkElement() = default;

LinkElement::LinkElement(std::shared_ptr<const common::Link> impl)
    : Element(impl), m_impl{std::move(impl)} {}

TextProperties LinkElement::textProperties() const {
  return m_impl->textProperties();
}

std::string LinkElement::href() const { return m_impl->href(); }

BookmarkElement::BookmarkElement() = default;

BookmarkElement::BookmarkElement(std::shared_ptr<const common::Bookmark> impl)
    : Element(impl), m_impl{std::move(impl)} {}

std::string BookmarkElement::name() const { return m_impl->name(); }

ImageElement::ImageElement() = default;

ImageElement::ImageElement(std::shared_ptr<const common::Image> impl)
    : Element(impl), m_impl{std::move(impl)} {}

ListElement::ListElement() = default;

ListElement::ListElement(std::shared_ptr<const common::List> impl)
    : Element(impl), m_impl{std::move(impl)} {}

ListItemElement::ListItemElement() = default;

ListItemElement::ListItemElement(std::shared_ptr<const common::ListItem> impl)
    : Element(impl), m_impl{std::move(impl)} {}

TableElement::TableElement() = default;

TableElement::TableElement(std::shared_ptr<const common::Table> impl)
    : Element(impl), m_impl{std::move(impl)} {}

TableColumnRange TableElement::columns() const {
  return TableColumnRange(TableColumnElement(m_impl->firstColumn()));
}

TableRowRange TableElement::rows() const {
  return TableRowRange(TableRowElement(m_impl->firstRow()));
}

TableColumnElement::TableColumnElement() = default;

TableColumnElement::TableColumnElement(
    std::shared_ptr<const common::TableColumn> impl)
    : Element(impl), m_impl{std::move(impl)} {}

TableColumnElement TableColumnElement::previousSibling() const {
  return Element::previousSibling().tableColumn();
}

TableColumnElement TableColumnElement::nextSibling() const {
  return Element::nextSibling().tableColumn();
}

TableRowElement::TableRowElement() = default;

TableRowElement::TableRowElement(std::shared_ptr<const common::TableRow> impl)
    : Element(impl), m_impl{std::move(impl)} {}

TableCellElement TableRowElement::firstChild() const {
  return Element::firstChild().tableCell();
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
    std::shared_ptr<const common::TableCell> impl)
    : Element(impl), m_impl{std::move(impl)} {}

TableCellElement TableCellElement::previousSibling() const {
  return Element::previousSibling().tableCell();
}

TableCellElement TableCellElement::nextSibling() const {
  return Element::nextSibling().tableCell();
}

std::uint32_t TableCellElement::rowSpan() const { return m_impl->rowSpan(); }

std::uint32_t TableCellElement::columnSpan() const {
  return m_impl->columnSpan();
}

} // namespace odr
