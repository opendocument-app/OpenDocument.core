#pragma once

#include <string>

namespace odr {
enum class FileType;
} // namespace odr

namespace odr::internal::ooxml {

/// The bytes of a new package of @p type, which `create_document` opens.
/// @throws UnsupportedFileType for a type with no blank package.
[[nodiscard]] std::string blank_package(FileType type);

} // namespace odr::internal::ooxml
