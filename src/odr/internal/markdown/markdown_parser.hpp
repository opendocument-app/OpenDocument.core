#pragma once

#include <odr/definitions.hpp>

#include <string_view>

namespace odr::internal::markdown {
class ElementRegistry;
class StyleRegistry;

/// Parses @p text — UTF-8 CommonMark plus the GitHub extensions — into
/// @p registry and @p style_registry, and returns the root element.
/// @throws std::runtime_error if md4c fails.
ElementIdentifier parse_tree(ElementRegistry &registry,
                             StyleRegistry &style_registry,
                             std::string_view text);

} // namespace odr::internal::markdown
