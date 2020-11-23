#include <common/DocumentElements.h>
#include <odr/DocumentElements.h>

namespace odr {

namespace {
template <typename R, typename E>
std::optional<R> convert(std::shared_ptr<const E> impl,
                         ElementType type) {
  if (!impl)
    return {};
  if (impl->type() != type)
    return {};
  return R(std::move(impl));
}
} // namespace

Element::Element(std::shared_ptr<const common::Element> impl)
    : m_impl{std::move(impl)} {}

bool Element::operator==(const Element &rhs) const {
  return m_impl == rhs.m_impl;
}

bool Element::operator!=(const Element &rhs) const {
  return m_impl != rhs.m_impl;
}

std::optional<Element> Element::parent() const {
  return common::Element::convert(m_impl->parent());
}

std::optional<Element> Element::firstChild() const {
  return common::Element::convert(m_impl->firstChild());
}

std::optional<Element> Element::previousSibling() const {
  return common::Element::convert(m_impl->previousSibling());
}

std::optional<Element> Element::nextSibling() const {
  return common::Element::convert(m_impl->nextSibling());
}

ElementType Element::type() const { return m_impl->type(); }

std::optional<Element> Element::unknown() const {
  return convert<Element>(m_impl, ElementType::UNKNOWN);
}

std::optional<TextElement> Element::text() const {
  return convert<TextElement>(
      std::dynamic_pointer_cast<const common::TextElement>(m_impl),
      ElementType::TEXT);
}

std::optional<Element> Element::lineBreak() const {
  return convert<Element>(m_impl, ElementType::LINE_BREAK);
}

std::optional<Element> Element::pageBreak() const {
  return convert<Element>(m_impl, ElementType::PAGE_BREAK);
}

std::optional<ParagraphElement> Element::paragraph() const {
  return convert<ParagraphElement>(
      std::dynamic_pointer_cast<const common::Paragraph>(m_impl),
      ElementType::PARAGRAPH);
}

std::optional<SpanElement> Element::span() const {
  return convert<SpanElement>(
      std::dynamic_pointer_cast<const common::Element>(m_impl),
      ElementType::SPAN);
}

std::optional<LinkElement> Element::link() const {
  return convert<LinkElement>(
      std::dynamic_pointer_cast<const common::Element>(m_impl),
      ElementType::LINK);
}

std::optional<ImageElement> Element::image() const {
  return convert<ImageElement>(
      std::dynamic_pointer_cast<const common::Element>(m_impl),
      ElementType::IMAGE);
}

std::optional<ListElement> Element::list() const {
  return convert<ListElement>(
      std::dynamic_pointer_cast<const common::Element>(m_impl),
      ElementType::LIST);
}

std::optional<TableElement> Element::table() const {
  return convert<TableElement>(
      std::dynamic_pointer_cast<const common::Table>(m_impl),
      ElementType::TABLE);
}

ElementSiblingIterator::ElementSiblingIterator(std::optional<Element> element) : m_element{std::move(element)} {}

ElementSiblingIterator &ElementSiblingIterator::operator++() {
  m_element = m_element->nextSibling();
  return *this;
}

ElementSiblingIterator ElementSiblingIterator::operator++(int) & {
  ElementSiblingIterator result = *this;
  operator++();
  return result;
}

Element &ElementSiblingIterator::operator*() {
  return *m_element;
}

Element *ElementSiblingIterator::operator->() {
  return &*m_element;
}

bool ElementSiblingIterator::operator==(const ElementSiblingIterator &rhs) const {
  return m_element == rhs.m_element;
}

bool ElementSiblingIterator::operator!=(const ElementSiblingIterator &rhs) const {
  return m_element != rhs.m_element;
}

ElementSiblingRange::ElementSiblingRange(std::optional<Element> begin, std::optional<Element> end) : m_begin{std::move(begin)}, m_end{std::move(end)} {}

ElementSiblingIterator ElementSiblingRange::begin()  { return ElementSiblingIterator(m_begin); }

ElementSiblingIterator ElementSiblingRange::end()  { return ElementSiblingIterator(m_end); }

TextElement::TextElement(std::shared_ptr<const common::TextElement> impl)
    : m_impl{std::move(impl)} {}

std::string TextElement::string() const { return m_impl->text(); }

ParagraphElement::ParagraphElement(
    std::shared_ptr<const common::Paragraph> impl)
    : m_impl{std::move(impl)} {}

SpanElement::SpanElement(std::shared_ptr<const common::Element> impl)
    : m_impl{std::move(impl)} {}

LinkElement::LinkElement(std::shared_ptr<const common::Element> impl)
    : m_impl{std::move(impl)} {}

ImageElement::ImageElement(std::shared_ptr<const common::Element> impl)
    : m_impl{std::move(impl)} {}

ListElement::ListElement(std::shared_ptr<const common::Element> impl)
    : m_impl{std::move(impl)} {}

TableElement::TableElement(std::shared_ptr<const common::Table> impl)
    : m_impl{std::move(impl)} {}

} // namespace odr
