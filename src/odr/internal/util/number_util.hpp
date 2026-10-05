#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace odr::internal::util::number {

/// Reads a decimal with a `.` separator in any host locale. Only whitespace may
/// surround it: a unit or a group separator is refused, not truncated.
[[nodiscard]] std::optional<double> parse(std::string_view text);

/// Formats significant digits without exponent notation, which CSS and SVG
/// lengths do not accept, and without trailing zeros. More digits than the
/// source carries show its noise: a `float` has about 7.
std::string to_string_significant(double value,
                                  std::int32_t significant_digits);

} // namespace odr::internal::util::number
