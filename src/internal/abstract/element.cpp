#include <internal/abstract/element.h>
#include <odr/element.h>

namespace odr::internal::abstract {

ElementType Slide::type() const { return ElementType::SLIDE; }

ElementType Sheet::type() const { return ElementType::SHEET; }

ElementType Page::type() const { return ElementType::PAGE; }

ElementType TextElement::type() const { return ElementType::TEXT; }

ElementType LinkElement::type() const { return ElementType::LINK; }

ElementType BookmarkElement::type() const { return ElementType::BOOKMARK; }

ElementType TableElement::type() const { return ElementType::TABLE; }

ElementType FrameElement::type() const { return ElementType::FRAME; }

ElementType ImageElement::type() const { return ElementType::IMAGE; }

ElementType RectElement::type() const { return ElementType::RECT; }

ElementType LineElement::type() const { return ElementType::LINE; }

ElementType CircleElement::type() const { return ElementType::CIRCLE; }

ElementType CustomShapeElement::type() const {
  return ElementType::CUSTOM_SHAPE;
}

} // namespace odr::internal::abstract
