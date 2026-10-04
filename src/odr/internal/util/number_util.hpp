#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace odr::internal::util::number {

/// Reads a decimal with a `.` separator and optional surrounding whitespace.
[[nodiscard]] std::optional<double> parse(std::string_view text);

/// Formats significant digits without exponent notation or trailing zeros.
std::string to_string_significant(double value,
                                  std::int32_t significant_digits);

} // namespace odr::internal::util::number
