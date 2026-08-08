#pragma once

#include <string>

namespace odr::internal::util::number {

/// Renders @p value with @p significant_digits significant digits, without
/// trailing zeros and never in scientific notation, which CSS and SVG lengths
/// do not accept. Asking for more digits than the source has shows its noise:
/// a `float` carries about 7, beyond that `68.55` becomes `68.550003`.
std::string to_string_significant(double value, int significant_digits);

} // namespace odr::internal::util::number
