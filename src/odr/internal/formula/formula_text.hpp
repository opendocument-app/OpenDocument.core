#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace odr::internal::formula {

/// The longest text a cell holds in both applications, in UTF-16 units.
inline constexpr std::size_t text_limit = 32767;

/// The UTF-16 units of the UTF-8 @p text.
[[nodiscard]] std::size_t utf16_length(std::string_view text);

/// @p text in UTF-16, the units both applications count a text in. Nothing
/// where it holds a character past the basic plane, whose two units a cut
/// could split.
[[nodiscard]] std::optional<std::u16string> utf16_of(std::string_view text);
[[nodiscard]] std::string utf8_of(const std::u16string &text);

/// @p text in upper or lower case. Nothing where it holds a letter outside
/// ASCII, Latin-1 and Latin Extended-A, or one whose case changes its length
/// (`ß`), since the applications map those apart.
[[nodiscard]] std::optional<std::u16string>
to_upper(const std::u16string &text);
[[nodiscard]] std::optional<std::u16string>
to_lower(const std::u16string &text);

/// @p text without case, for a comparison that ignores it. Nothing as for
/// @ref to_lower.
[[nodiscard]] std::optional<std::string> folded(std::string_view text);

} // namespace odr::internal::formula
