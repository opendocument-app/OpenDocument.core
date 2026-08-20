#pragma once

#include <odr/definitions.hpp>

namespace odr::internal::abstract {
class ReadableFilesystem;
}

namespace odr::internal::oldms::presentation {
class ElementRegistry;
class StyleRegistry;

/// Parses the presentation into `registry`'s slide/frame/paragraph/span/text
/// elements; fills `style_registry` with the resolved character styles.
ElementIdentifier parse_tree(ElementRegistry &registry,
                             StyleRegistry &style_registry,
                             const abstract::ReadableFilesystem &files);

/// Whether the presentation is encrypted, from `CurrentUserAtom.headerToken`
/// ([MS-PPT] 2.3.2). A `/Current User` stream that is missing or too short
/// reads as not encrypted — parsing then fails on its own.
[[nodiscard]] bool
password_encrypted(const abstract::ReadableFilesystem &files);

} // namespace odr::internal::oldms::presentation
