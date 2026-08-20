#pragma once

#include <odr/definitions.hpp>

#include <optional>

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

/// Whether the document is encrypted or obfuscated, from `FibBase.fEncrypted`
/// ([MS-DOC] 2.5.2). The FIB itself stays in the clear, which is what makes
/// this readable at all. Nothing where the `/WordDocument` stream is missing or
/// too short to carry a FIB: that is not an answer, and saying "not encrypted"
/// would be claiming one.
[[nodiscard]] std::optional<bool>
password_encrypted(const abstract::ReadableFilesystem &files);

} // namespace odr::internal::oldms::text
