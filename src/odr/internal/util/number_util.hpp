#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace odr::internal::util::number {

/// Reads @p text as a decimal number with a `.` separator, whatever the host's
/// locale — a german one would read `1234.5` as `1234`. Only blanks may
/// surround it: a unit or a group separator is refused, not truncated.
[[nodiscard]] std::optional<double> parse(std::string_view text);

/// Renders @p value with @p significant_digits significant digits, without
/// trailing zeros, never in scientific notation, which CSS and SVG lengths do
/// not accept, and never in the host's locale, where a german one would write
/// `1,5`. Asking for more digits than the source has shows its noise: a
/// `float` carries about 7, beyond that `68.55` becomes `68.550003`.
std::string to_string_significant(double value, int significant_digits);

} // namespace odr::internal::util::number
