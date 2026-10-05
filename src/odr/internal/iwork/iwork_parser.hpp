#pragma once

#include <odr/definitions.hpp>

namespace odr::internal::abstract {
class ReadableFilesystem;
} // namespace odr::internal::abstract

namespace odr::internal::iwork {
class ElementRegistry;

/// Parses a Pages body and returns its root element ID.
ElementIdentifier parse_pages_tree(ElementRegistry &registry,
                                   const abstract::ReadableFilesystem &files);

/// Parses Keynote slides and text frames; returns the root element ID.
ElementIdentifier parse_keynote_tree(ElementRegistry &registry,
                                     const abstract::ReadableFilesystem &files);

/// Parses Numbers tables as separate sheets; returns the root element ID.
ElementIdentifier parse_numbers_tree(ElementRegistry &registry,
                                     const abstract::ReadableFilesystem &files);

} // namespace odr::internal::iwork
