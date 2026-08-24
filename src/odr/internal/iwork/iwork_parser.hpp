#pragma once

#include <odr/definitions.hpp>

namespace odr::internal::abstract {
class ReadableFilesystem;
} // namespace odr::internal::abstract

namespace odr::internal::iwork {
class ElementRegistry;

/// Parses the body of a `.pages` package into root → paragraph → text
/// elements.
/// \return the root element id.
ElementIdentifier parse_pages_tree(ElementRegistry &registry,
                                   const abstract::ReadableFilesystem &files);

} // namespace odr::internal::iwork
