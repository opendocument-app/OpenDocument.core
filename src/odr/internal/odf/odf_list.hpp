#pragma once

#include <odr/definitions.hpp>

namespace odr::internal::odf {
class ElementRegistry;
class StyleRegistry;

/// Stamps every list and item, in document order — which is what the counters
/// need.
void resolve_list_numbering(ElementRegistry &registry,
                            const StyleRegistry &styles,
                            ElementIdentifier root_id);

} // namespace odr::internal::odf
