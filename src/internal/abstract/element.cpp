#include <internal/abstract/element.h>

namespace odr::internal::abstract {

ElementType TextElement::type() const { return ElementType::TEXT; }

ElementType DenseTableElement::type() const { return ElementType::TABLE; }

ElementType SparseTableElement::type() const { return ElementType::TABLE; }

ElementType ImageElement::type() const { return ElementType::IMAGE; }

} // namespace odr::internal::abstract
