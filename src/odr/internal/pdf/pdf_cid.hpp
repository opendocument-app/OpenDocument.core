#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace odr::internal::pdf {

/// Decodes a predefined Unicode or legacy CJK CMap to UTF-8, omitting unmapped
/// codes. Returns `nullopt` for unknown names and `Identity-H/V`.
[[nodiscard]] std::optional<std::string>
translate_predefined_cmap(std::string_view name, const std::string &codes);

/// Maps a CID through its `/CIDSystemInfo` collection; returns `nullopt`
/// for unknown collections or unmapped CIDs.
[[nodiscard]] std::optional<char32_t> cid_to_unicode(std::string_view registry,
                                                     std::string_view ordering,
                                                     std::uint32_t cid);

} // namespace odr::internal::pdf
