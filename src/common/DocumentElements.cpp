#include <common/DocumentElements.h>
#include <odr/Document.h>
#include <odr/DocumentElements.h>

namespace odr::common {

ElementType TextElement::type() const { return ElementType::TEXT; }

ElementType Paragraph::type() const { return ElementType::PARAGRAPH; }

ElementType Span::type() const { return ElementType::SPAN; }

ElementType Link::type() const { return ElementType::LINK; }

ElementType Image::type() const { return ElementType::IMAGE; }

ElementType List::type() const { return ElementType::LIST; }

ElementType Table::type() const { return ElementType::TABLE; }

} // namespace odr::common
