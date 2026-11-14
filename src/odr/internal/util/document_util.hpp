#pragma once

#include <odr/document_element.hpp>

namespace odr::internal::util::document {

/// The first paragraph under @p element, depth first; null where it holds
/// none.
[[nodiscard]] Element first_paragraph_in(const Element &element);

} // namespace odr::internal::util::document
