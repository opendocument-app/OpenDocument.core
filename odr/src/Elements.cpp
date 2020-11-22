#include <common/AbstractDocument.h>
#include <odr/Elements.h>

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

Element::Element(std::shared_ptr<const common::AbstractElement> impl)
    : m_impl{std::move(impl)} {}

std::optional<Element> Element::parent() const {
  return common::AbstractElement::convert(m_impl->parent());
}

std::optional<Element> Element::firstChild() const {
  return common::AbstractElement::convert(m_impl->firstChild());
}

std::optional<Element> Element::previousSibling() const {
  return common::AbstractElement::convert(m_impl->previousSibling());
}

std::optional<Element> Element::nextSibling() const {
  return common::AbstractElement::convert(m_impl->nextSibling());
}

ElementType Element::type() const { return m_impl->type(); }

std::optional<Element> Element::unknown() const {
  return convert<Element>(m_impl, ElementType::UNKNOWN);
}

std::optional<TextElement> Element::text() const {
  return convert<TextElement>(
      std::dynamic_pointer_cast<const common::AbstractText>(m_impl),
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
      std::dynamic_pointer_cast<const common::AbstractParagraph>(m_impl),
      ElementType::PARAGRAPH);
}

std::optional<SpanElement> Element::span() const {
  return convert<SpanElement>(
      std::dynamic_pointer_cast<const common::AbstractElement>(m_impl),
      ElementType::SPAN);
}

std::optional<LinkElement> Element::link() const {
  return convert<LinkElement>(
      std::dynamic_pointer_cast<const common::AbstractElement>(m_impl),
      ElementType::LINK);
}

std::optional<ImageElement> Element::image() const {
  return convert<ImageElement>(
      std::dynamic_pointer_cast<const common::AbstractElement>(m_impl),
      ElementType::IMAGE);
}

std::optional<ListElement> Element::list() const {
  return convert<ListElement>(
      std::dynamic_pointer_cast<const common::AbstractElement>(m_impl),
      ElementType::LIST);
}

std::optional<TableElement> Element::table() const {
  return convert<TableElement>(
      std::dynamic_pointer_cast<const common::AbstractTable>(m_impl),
      ElementType::TABLE);
}

TextElement::TextElement(std::shared_ptr<const common::AbstractText> impl)
    : m_impl{std::move(impl)} {}

std::string TextElement::string() const { return m_impl->text(); }

ParagraphElement::ParagraphElement(
    std::shared_ptr<const common::AbstractParagraph> impl)
    : m_impl{std::move(impl)} {}

SpanElement::SpanElement(std::shared_ptr<const common::AbstractElement> impl)
    : m_impl{std::move(impl)} {}

LinkElement::LinkElement(std::shared_ptr<const common::AbstractElement> impl)
    : m_impl{std::move(impl)} {}

ImageElement::ImageElement(std::shared_ptr<const common::AbstractElement> impl)
    : m_impl{std::move(impl)} {}

ListElement::ListElement(std::shared_ptr<const common::AbstractElement> impl)
    : m_impl{std::move(impl)} {}

TableElement::TableElement(std::shared_ptr<const common::AbstractTable> impl)
    : m_impl{std::move(impl)} {}

} // namespace odr
