#include <common/DocumentElements.h>
#include <odr/Document.h>
#include <odr/DocumentElements.h>

namespace odr::common {

std::optional<odr::Element>
Element::convert(std::shared_ptr<const common::Element> element) {
  if (!element)
    return {};
  return odr::Element(std::move(element));
}

ElementType TextElement::type() const {
  return ElementType::TEXT;
}

ElementType Paragraph::type() const {
  return ElementType::PARAGRAPH;
}

ElementType Span::type() const {
  return ElementType::SPAN;
}

ElementType Link::type() const {
  return ElementType::LINK;
}

ElementType Image::type() const {
  return ElementType::IMAGE;
}

ElementType List::type() const {
  return ElementType::LIST;
}

ElementType Table::type() const {
  return ElementType::TABLE;
}

}
