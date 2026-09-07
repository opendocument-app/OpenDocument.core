#include <odr/internal/util/number_util.hpp>

#include <algorithm>
#include <cmath>
#include <ios>
#include <istream>
#include <locale>
#include <sstream>

#include <fmt/format.h>

namespace odr::internal::util {

std::optional<double> number::parse(const std::string_view text) {
  std::istringstream stream{std::string(text)};
  // every format we decode writes a `.`, whatever the host's locale is
  stream.imbue(std::locale::classic());

  double value = 0;
  stream >> value;
  if (stream.fail()) {
    return {};
  }
  // `>>` stops at the first character it cannot use rather than failing, which
  // would take `1,5` for `1`
  return (stream >> std::ws).eof() ? std::optional(value) : std::nullopt;
}

std::string number::to_string_significant(const double value,
                                          const int significant_digits) {
  if (!std::isfinite(value)) {
    return fmt::format("{}", value);
  }

  // `{:.Nf}` counts decimals, not significant digits, so shift by the integer
  // part; clamped because a denormal or a huge value would blow up
  int integer_digits = 1;
  if (value != 0.0) {
    integer_digits =
        static_cast<int>(std::floor(std::log10(std::abs(value)))) + 1;
  }
  const int decimals = std::clamp(significant_digits - integer_digits, 0, 15);

  std::string result = fmt::format("{:.{}f}", value, decimals);

  if (result.find('.') != std::string::npos) {
    result.erase(result.find_last_not_of('0') + 1);
    if (!result.empty() && result.back() == '.') {
      result.pop_back();
    }
  }
  return result;
}

} // namespace odr::internal::util
