#pragma once

#include <odr/definitions.hpp>

#include <iosfwd>

namespace odr::internal::rtf {
class ElementRegistry;

/// Parses the rtf byte stream into root → paragraph → (text | line break)
/// elements. Character and paragraph formatting, tables and pictures are not
/// modelled yet; see `PLAN.md`.
/// \return the root element id.
ElementIdentifier parse_tree(ElementRegistry &registry, std::istream &in);

} // namespace odr::internal::rtf
