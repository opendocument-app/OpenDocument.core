#pragma once

#include <cmath>
#include <concepts>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

namespace odr::internal::util::number {

/// Reads a decimal with a `.` separator in any host locale. Only whitespace may
/// surround it: a unit or a group separator is refused, not truncated.
[[nodiscard]] std::optional<double> parse(std::string_view text);

/// Formats significant digits without exponent notation, which CSS and SVG
/// lengths do not accept, and without trailing zeros. More digits than the
/// source carries show its noise: a `float` has about 7.
std::string to_string_significant(double value,
                                  std::int32_t significant_digits);

/// @p value as a `T`, or nothing if it is not finite, has a fraction or does
/// not fit.
template <std::integral T>
[[nodiscard]] std::optional<T> to_integer(const double value) {
  // 2^digits is exact as a double, where `max()` may round up for 64 bits.
  const double limit = std::ldexp(1.0, std::numeric_limits<T>::digits);
  if (!std::isfinite(value) || std::trunc(value) != value || value >= limit ||
      value < (std::is_signed_v<T> ? -limit : 0.0)) {
    return std::nullopt;
  }
  return static_cast<T>(value);
}

} // namespace odr::internal::util::number
