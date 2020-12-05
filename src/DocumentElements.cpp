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

ElementSiblingRange Element::children() const {
  return ElementSiblingRange(firstChild());
}

ElementType Element::type() const { return m_impl->type(); }

Element Element::unknown() const {
  if (type() == ElementType::UNKNOWN)
    return {};
  return *this;
}

TextElement Element::text() const {
  return TextElement(
      std::dynamic_pointer_cast<const common::TextElement>(m_impl));
}

Element Element::lineBreak() const {
  if (type() == ElementType::LINE_BREAK)
    return {};
  return *this;
}

Element Element::pageBreak() const {
  if (type() == ElementType::PAGE_BREAK)
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

ImageElement Element::image() const {
  return ImageElement(std::dynamic_pointer_cast<const common::Image>(m_impl));
}

ListElement Element::list() const {
  return ListElement(std::dynamic_pointer_cast<const common::List>(m_impl));
}

TableElement Element::table() const {
  return TableElement(std::dynamic_pointer_cast<const common::Table>(m_impl));
}

ElementSiblingIterator::ElementSiblingIterator(Element element)
    : m_element{std::move(element)} {}

ElementSiblingIterator &ElementSiblingIterator::operator++() {
  m_element = m_element.nextSibling();
  return *this;
}

ElementSiblingIterator ElementSiblingIterator::operator++(int) & {
  ElementSiblingIterator result = *this;
  operator++();
  return result;
}

Element &ElementSiblingIterator::operator*() { return m_element; }

Element *ElementSiblingIterator::operator->() { return &m_element; }

bool ElementSiblingIterator::operator==(
    const ElementSiblingIterator &rhs) const {
  return m_element == rhs.m_element;
}

bool ElementSiblingIterator::operator!=(
    const ElementSiblingIterator &rhs) const {
  return m_element != rhs.m_element;
}

ElementSiblingRange::ElementSiblingRange(Element begin)
    : ElementSiblingRange(std::move(begin), {}) {}

ElementSiblingRange::ElementSiblingRange(Element begin, Element end)
    : m_begin{std::move(begin)}, m_end{std::move(end)} {}

ElementSiblingIterator ElementSiblingRange::begin() const {
  return ElementSiblingIterator(m_begin);
}

ElementSiblingIterator ElementSiblingRange::end() const {
  return ElementSiblingIterator(m_end);
}

Element ElementSiblingRange::front() const { return m_begin; }

TextElement::TextElement() = default;

TextElement::TextElement(std::shared_ptr<const common::TextElement> impl)
    : m_impl{std::move(impl)} {}

TextElement::operator bool() const { return m_impl.operator bool(); }

std::string TextElement::string() const { return m_impl->text(); }

ParagraphElement::ParagraphElement() = default;

ParagraphElement::ParagraphElement(
    std::shared_ptr<const common::Paragraph> impl)
    : m_impl{std::move(impl)} {}

ParagraphElement::operator bool() const { return m_impl.operator bool(); }

ParagraphProperties ParagraphElement::paragraphProperties() const {
  return m_impl->paragraphProperties();
}

TextProperties ParagraphElement::textProperties() const {
  return m_impl->textProperties();
}

SpanElement::SpanElement() = default;

SpanElement::SpanElement(std::shared_ptr<const common::Span> impl)
    : m_impl{std::move(impl)} {}

SpanElement::operator bool() const { return m_impl.operator bool(); }

TextProperties SpanElement::textProperties() const {
  return m_impl->textProperties();
}

LinkElement::LinkElement() = default;

LinkElement::LinkElement(std::shared_ptr<const common::Element> impl)
    : m_impl{std::move(impl)} {}

LinkElement::operator bool() const { return m_impl.operator bool(); }

ImageElement::ImageElement() = default;

ImageElement::ImageElement(std::shared_ptr<const common::Element> impl)
    : m_impl{std::move(impl)} {}

ImageElement::operator bool() const { return m_impl.operator bool(); }

ListElement::ListElement() = default;

ListElement::ListElement(std::shared_ptr<const common::Element> impl)
    : m_impl{std::move(impl)} {}

ListElement::operator bool() const { return m_impl.operator bool(); }

TableElement::TableElement() = default;

TableElement::TableElement(std::shared_ptr<const common::Table> impl)
    : m_impl{std::move(impl)} {}

TableElement::operator bool() const { return m_impl.operator bool(); }

} // namespace odr
