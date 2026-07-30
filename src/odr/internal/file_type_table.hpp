#pragma once

#include <odr/file.hpp>

#include <span>
#include <string_view>

namespace odr::internal::file_type_table {

/// Everything the library statically knows about one file type.
struct Row final {
  FileType type{FileType::unknown};
  std::string_view name;                        ///< as `file_type_to_string`
  std::span<const std::string_view> extensions; ///< canonical one first
  std::span<const std::string_view> mimetypes;  ///< canonical one first
  FileCategory category{FileCategory::unknown};
  DocumentType document_type{DocumentType::unknown};
  FileTypeCapabilities capabilities;
};

/// The whole table, one row per @ref FileType, in declaration order.
[[nodiscard]] std::span<const Row> rows() noexcept;

/// The row for @p type, or `nullptr` if there is none.
[[nodiscard]] const Row *find(FileType type) noexcept;

/// The row listing @p extension among its extensions, or `nullptr`.
[[nodiscard]] const Row *find_by_extension(std::string_view extension) noexcept;

/// The row listing @p mimetype among its MIME types, or `nullptr`.
[[nodiscard]] const Row *find_by_mimetype(std::string_view mimetype) noexcept;

} // namespace odr::internal::file_type_table
