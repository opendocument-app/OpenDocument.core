#pragma once

#include <string>

namespace odr::internal::util::number {

/// Renders @p value with @p significant_digits significant digits, always in
/// positional notation and without trailing zeros.
///
/// Unlike the iostream/printf default (`%g`), this never switches to
/// scientific notation — CSS and SVG lengths do not accept it. Non-finite
/// values are passed through to the stream default.
///
/// @p significant_digits is what bounds the noise: a value that came from a
/// `float` carries about 7 digits (`FLT_DIG` is 6), so asking for more digits
/// than the source has turns `68.55` into `68.550003`.
std::string to_string_significant(double value, int significant_digits);

} // namespace odr::internal::util::number
