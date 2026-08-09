#pragma once

#include <odr/file.hpp>

#include <span>
#include <string_view>

namespace odr::internal::encoding::text_encoding_table {

/// Everything the library statically knows about one text encoding.
struct Row final {
  TextEncoding encoding{TextEncoding::unknown};
  std::span<const std::string_view> names; ///< canonical one first
  bool decodable{}; ///< @ref encoding::to_utf8 handles it
};

/// The whole table, one row per @ref TextEncoding, in declaration order.
[[nodiscard]] std::span<const Row> rows() noexcept;

/// The row for @p encoding, or `nullptr` if there is none.
[[nodiscard]] const Row *find(TextEncoding encoding) noexcept;

/// The row listing @p name among its names, or `nullptr`. Matching ignores
/// case and any `-`, `_` or space, so `windows-1252`, `WINDOWS_1252` and
/// `cp1252` all land on the same row.
[[nodiscard]] const Row *find_by_name(std::string_view name) noexcept;

} // namespace odr::internal::encoding::text_encoding_table
