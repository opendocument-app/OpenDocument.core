#pragma once

#include <string>
#include <string_view>

namespace odr {
enum class FileType;
} // namespace odr

namespace odr::internal {
enum class BlankPlaceholderKind;
} // namespace odr::internal

namespace odr::internal::odf {

/// The `presentation:object` and `presentation:class` of a placeholder of a
/// built-in layout (ODF 1.3 part 3, 19.427 and 19.388).
[[nodiscard]] std::string_view presentation_class(BlankPlaceholderKind kind);

/// The bytes of a new package of @p type, which `create_document` opens.
/// @throws UnsupportedFileType for a type with no blank package.
[[nodiscard]] std::string blank_package(FileType type);

} // namespace odr::internal::odf
