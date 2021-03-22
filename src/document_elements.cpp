#include <internal/abstract/document.h>
#include <odr/document.h>
#include <odr/document_elements.h>
#include <odr/document_style.h>
#include <odr/file.h>
#include <odr/table_dimensions.h>

namespace odr {

Element::Element() = default;

Element::Element(std::shared_ptr<const internal::abstract::Document> impl,
                 const std::uint64_t id)
    : m_impl{std::move(impl)}, m_id{id} {}

bool Element::operator==(const Element &rhs) const {
  return m_impl == rhs.m_impl && m_id == rhs.m_id;
}

bool Element::operator!=(const Element &rhs) const {
  return m_impl != rhs.m_impl || m_id != rhs.m_id;
}

Element::operator bool() const { return m_id != 0; }

ElementType Element::type() const {
  if (!m_impl) {
    return ElementType::NONE;
  }
  return m_impl->element_type(m_id);
}

Element Element::parent() const {
  if (!m_impl) {
    return Element();
  }
  return Element(m_impl, m_impl->element_parent(m_id));
}

Element Element::first_child() const {
  if (!m_impl) {
    return Element();
  }
  return Element(m_impl, m_impl->element_first_child(m_id));
}

Element Element::previous_sibling() const {
  if (!m_impl) {
    return Element();
  }
  return Element(m_impl, m_impl->element_previous_sibling(m_id));
}

Element Element::next_sibling() const {
  if (!m_impl) {
    return Element();
  }
  return Element(m_impl, m_impl->element_next_sibling(m_id));
}

ElementRange Element::children() const { return ElementRange(first_child()); }

ElementPropertyValue Element::property(ElementProperty property) const {
  return ElementPropertyValue(m_impl, m_id, property);
}

SlideElement Element::slide() const { return SlideElement(m_impl, m_id); }

SheetElement Element::sheet() const { return SheetElement(m_impl, m_id); }

PageElement Element::page() const { return PageElement(m_impl, m_id); }

TextElement Element::text() const { return TextElement(m_impl, m_id); }

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
  return ParagraphElement(m_impl, m_id);
}

SpanElement Element::span() const { return SpanElement(m_impl, m_id); }

LinkElement Element::link() const { return LinkElement(m_impl, m_id); }

BookmarkElement Element::bookmark() const {
  return BookmarkElement(m_impl, m_id);
}

ListElement Element::list() const { return ListElement(m_impl, m_id); }

ListItemElement Element::list_item() const {
  return ListItemElement(m_impl, m_id);
}

TableElement Element::table() const { return TableElement(m_impl, m_id); }

TableColumnElement Element::table_column() const {
  return TableColumnElement(m_impl, m_id);
}

TableRowElement Element::table_row() const {
  return TableRowElement(m_impl, m_id);
}

TableCellElement Element::table_cell() const {
  return TableCellElement(m_impl, m_id);
}

FrameElement Element::frame() const { return FrameElement(m_impl, m_id); }

ImageElement Element::image() const { return ImageElement(m_impl, m_id); }

RectElement Element::rect() const { return RectElement(m_impl, m_id); }

LineElement Element::line() const { return LineElement(m_impl, m_id); }

CircleElement Element::circle() const { return CircleElement(m_impl, m_id); }

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
    std::shared_ptr<const internal::abstract::Document> impl,
    const std::uint64_t id)
    : Element(std::move(impl), id) {}

SlideElement SlideElement::previous_sibling() const {
  return Element::previous_sibling().slide();
}

SlideElement SlideElement::next_sibling() const {
  return Element::next_sibling().slide();
}

std::string SlideElement::name() const {
  return m_impl->element_string_property(m_id, ElementProperty::NAME);
}

std::string SlideElement::notes() const {
  return m_impl->element_string_property(m_id, ElementProperty::NOTES);
}

PageStyle SlideElement::page_style() const { return PageStyle(m_impl, m_id); }

SheetElement::SheetElement() = default;

SheetElement::SheetElement(
    std::shared_ptr<const internal::abstract::Document> impl,
    const std::uint64_t id)
    : Element(std::move(impl), id) {}

SheetElement SheetElement::previous_sibling() const {
  return Element::previous_sibling().sheet();
}

SheetElement SheetElement::next_sibling() const {
  return Element::next_sibling().sheet();
}

std::string SheetElement::name() const {
  return m_impl->element_string_property(m_id, ElementProperty::NAME);
}

TableElement SheetElement::table() const {
  if (!m_impl) {
    return TableElement();
  }
  return TableElement(m_impl, m_id); // TODO
}

PageElement::PageElement() = default;

PageElement::PageElement(
    std::shared_ptr<const internal::abstract::Document> impl,
    const std::uint64_t id)
    : Element(std::move(impl), id) {}

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
  return m_impl->element_string_property(m_id, ElementProperty::NAME);
}

PageStyle PageElement::page_style() const { return PageStyle(m_impl, m_id); }

TextElement::TextElement() = default;

TextElement::TextElement(
    std::shared_ptr<const internal::abstract::Document> impl,
    const std::uint64_t id)
    : Element(std::move(impl), id) {}

std::string TextElement::string() const {
  if (!m_impl) {
    return "";
  }
  return m_impl->element_string_property(m_id, ElementProperty::TEXT);
}

ParagraphElement::ParagraphElement() = default;

ParagraphElement::ParagraphElement(
    std::shared_ptr<const internal::abstract::Document> impl,
    const std::uint64_t id)
    : Element(std::move(impl), id) {}

SpanElement::SpanElement() = default;

SpanElement::SpanElement(
    std::shared_ptr<const internal::abstract::Document> impl,
    const std::uint64_t id)
    : Element(std::move(impl), id) {}

LinkElement::LinkElement() = default;

LinkElement::LinkElement(
    std::shared_ptr<const internal::abstract::Document> impl,
    const std::uint64_t id)
    : Element(std::move(impl), id) {}

std::string LinkElement::href() const {
  if (!m_impl) {
    return "";
  }
  return m_impl->element_string_property(m_id, ElementProperty::HREF);
}

BookmarkElement::BookmarkElement() = default;

BookmarkElement::BookmarkElement(
    std::shared_ptr<const internal::abstract::Document> impl,
    const std::uint64_t id)
    : Element(std::move(impl), id) {}

std::string BookmarkElement::name() const {
  if (!m_impl) {
    return "";
  }
  return m_impl->element_string_property(m_id, ElementProperty::NAME);
}

ListElement::ListElement() = default;

ListElement::ListElement(
    std::shared_ptr<const internal::abstract::Document> impl,
    const std::uint64_t id)
    : Element(std::move(impl), id) {}

ListItemElement::ListItemElement() = default;

ListItemElement::ListItemElement(
    std::shared_ptr<const internal::abstract::Document> impl,
    const std::uint64_t id)
    : Element(std::move(impl), id) {}

TableElement::TableElement() = default;

TableElement::TableElement(
    std::shared_ptr<const internal::abstract::Document> impl,
    const std::uint64_t id)
    : Element(std::move(impl), id) {}

TableDimensions TableElement::dimensions() const {
  if (!m_impl) {
    return TableDimensions(); // TODO
  }
  // TODO limit?
  return m_impl->table_dimensions(m_id, 0, 0);
}

TableColumnRange TableElement::columns() const {
  if (!m_impl) {
    return TableColumnRange();
  }
  return TableColumnRange(); // TODO
}

TableRowRange TableElement::rows() const {
  if (!m_impl) {
    return TableRowRange();
  }
  return TableRowRange(); // TODO
}

TableColumnElement::TableColumnElement() = default;

TableColumnElement::TableColumnElement(
    std::shared_ptr<const internal::abstract::Document> impl,
    const std::uint64_t id)
    : Element(std::move(impl), id) {}

TableColumnElement TableColumnElement::previous_sibling() const {
  return Element::previous_sibling().table_column();
}

TableColumnElement TableColumnElement::next_sibling() const {
  return Element::next_sibling().table_column();
}

TableRowElement::TableRowElement() = default;

TableRowElement::TableRowElement(
    std::shared_ptr<const internal::abstract::Document> impl,
    const std::uint64_t id)
    : Element(std::move(impl), id) {}

TableCellElement TableRowElement::first_child() const {
  if (!m_impl) {
    return TableCellElement();
  }
  return Element::first_child().table_cell();
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
    std::shared_ptr<const internal::abstract::Document> impl,
    const std::uint64_t id)
    : Element(std::move(impl), id) {}

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
  return m_impl->element_uint32_property(m_id,
                                         ElementProperty::TABLE_CELL_ROW_SPAN);
}

std::uint32_t TableCellElement::column_span() const {
  if (!m_impl) {
    return 0;
  }
  return m_impl->element_uint32_property(
      m_id, ElementProperty::TABLE_CELL_COLUMN_SPAN);
}

FrameElement::FrameElement() = default;

FrameElement::FrameElement(
    std::shared_ptr<const internal::abstract::Document> impl,
    const std::uint64_t id)
    : Element(std::move(impl), id) {}

std::string FrameElement::anchor_type() const {
  if (!m_impl) {
    return "";
  }
  return m_impl->element_string_property(m_id, ElementProperty::ANCHOR_TYPE);
}

std::string FrameElement::width() const {
  if (!m_impl) {
    return "";
  }
  return m_impl->element_string_property(m_id, ElementProperty::WIDTH);
}

std::string FrameElement::height() const {
  if (!m_impl) {
    return "";
  }
  return m_impl->element_string_property(m_id, ElementProperty::HEIGHT);
}

std::string FrameElement::z_index() const {
  if (!m_impl) {
    return "";
  }
  return m_impl->element_string_property(m_id, ElementProperty::Z_INDEX);
}

ImageElement::ImageElement() = default;

ImageElement::ImageElement(
    std::shared_ptr<const internal::abstract::Document> impl,
    const std::uint64_t id)
    : Element(std::move(impl), id) {}

bool ImageElement::internal() const {
  if (!m_impl) {
    return false;
  }
  return m_impl->element_bool_property(m_id, ElementProperty::IMAGE_INTERNAL);
}

std::string ImageElement::href() const {
  if (!m_impl) {
    return "";
  }
  return m_impl->element_string_property(m_id, ElementProperty::HREF);
}

File ImageElement::image_file() const {
  if (!m_impl) {
    // TODO there is no "empty" file
    return File(std::shared_ptr<internal::abstract::File>());
  }
  return File(m_impl->image_file(m_id));
}

RectElement::RectElement() = default;

RectElement::RectElement(
    std::shared_ptr<const internal::abstract::Document> impl,
    const std::uint64_t id)
    : Element(std::move(impl), id) {}

std::string RectElement::x() const {
  if (!m_impl) {
    return "";
  }
  return m_impl->element_string_property(m_id, ElementProperty::X);
}

std::string RectElement::y() const {
  if (!m_impl) {
    return "";
  }
  return m_impl->element_string_property(m_id, ElementProperty::Y);
}

std::string RectElement::width() const {
  if (!m_impl) {
    return "";
  }
  return m_impl->element_string_property(m_id, ElementProperty::WIDTH);
}

std::string RectElement::height() const {
  if (!m_impl) {
    return "";
  }
  return m_impl->element_string_property(m_id, ElementProperty::HEIGHT);
}

LineElement::LineElement() = default;

LineElement::LineElement(
    std::shared_ptr<const internal::abstract::Document> impl,
    const std::uint64_t id)
    : Element(std::move(impl), id) {}

std::string LineElement::x1() const {
  if (!m_impl) {
    return "";
  }
  return m_impl->element_string_property(m_id, ElementProperty::X1);
}

std::string LineElement::y1() const {
  if (!m_impl) {
    return "";
  }
  return m_impl->element_string_property(m_id, ElementProperty::Y1);
}

std::string LineElement::x2() const {
  if (!m_impl) {
    return "";
  }
  return m_impl->element_string_property(m_id, ElementProperty::X2);
}

std::string LineElement::y2() const {
  if (!m_impl) {
    return "";
  }
  return m_impl->element_string_property(m_id, ElementProperty::Y2);
}

CircleElement::CircleElement() = default;

CircleElement::CircleElement(
    std::shared_ptr<const internal::abstract::Document> impl,
    const std::uint64_t id)
    : Element(std::move(impl), id) {}

std::string CircleElement::x() const {
  if (!m_impl) {
    return "";
  }
  return m_impl->element_string_property(m_id, ElementProperty::X);
}

std::string CircleElement::y() const {
  if (!m_impl) {
    return "";
  }
  return m_impl->element_string_property(m_id, ElementProperty::Y);
}

std::string CircleElement::width() const {
  if (!m_impl) {
    return "";
  }
  return m_impl->element_string_property(m_id, ElementProperty::WIDTH);
}

std::string CircleElement::height() const {
  if (!m_impl) {
    return "";
  }
  return m_impl->element_string_property(m_id, ElementProperty::HEIGHT);
}

} // namespace odr
