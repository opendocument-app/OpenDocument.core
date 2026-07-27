#include <odr/internal/util/number_util.hpp>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace odr::internal::util {

std::string number::to_string_significant(const double value,
                                          const int significant_digits) {
  if (!std::isfinite(value)) {
    std::ostringstream ss;
    ss << value;
    return ss.str();
  }

  // `std::fixed` counts decimals, not significant digits, so shift by the size
  // of the integer part. Clamped: a denormal would otherwise ask for hundreds
  // of decimals, and a huge value for a negative count.
  int integer_digits = 1;
  if (value != 0.0) {
    integer_digits =
        static_cast<int>(std::floor(std::log10(std::abs(value)))) + 1;
  }
  const int decimals = std::clamp(significant_digits - integer_digits, 0, 15);

  std::ostringstream ss;
  ss << std::fixed << std::setprecision(decimals) << value;
  std::string result = ss.str();

  if (result.find('.') != std::string::npos) {
    result.erase(result.find_last_not_of('0') + 1);
    if (!result.empty() && result.back() == '.') {
      result.pop_back();
    }
  }
  return result;
}

} // namespace odr::internal::util
