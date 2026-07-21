#pragma once

#include <odr/definitions.hpp>

namespace odr::internal::abstract {
class ReadableFilesystem;
}

namespace odr::internal::oldms::text {
class ElementRegistry;
class StyleRegistry;

/// Parses the `/WordDocument` stream into root → paragraph → span → text
/// elements; fills `style_registry` with the resolved character styles.
/// \return the root element id.
ElementIdentifier parse_tree(ElementRegistry &registry,
                             StyleRegistry &style_registry,
                             const abstract::ReadableFilesystem &files);

} // namespace odr::internal::oldms::text
