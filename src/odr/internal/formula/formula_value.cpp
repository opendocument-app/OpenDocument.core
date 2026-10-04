#include <odr/internal/formula/formula_value.hpp>

#include <odr/internal/util/string_util.hpp>

#include <fmt/format.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <utility>

namespace odr::internal::formula {

namespace str = util::string;

namespace {

/// The length of the run of ASCII digits at @p at of @p text.
std::size_t digits_at(const std::string_view text, const std::size_t at) {
  std::size_t end = at;
  while (end < text.size() && str::is_ascii_digit(text[end])) {
    ++end;
  }
  return end - at;
}

/// `1`, `-1.5`, `.5`, `1.`, `1e2`, `+1E-2`: the number grammar both
/// applications read without the locale. Nothing where @p text is not one.
std::optional<double> plain_number(const std::string_view text) {
  std::size_t at = 0;
  if (at < text.size() && (text[at] == '+' || text[at] == '-')) {
    ++at;
  }
  const std::size_t integer = digits_at(text, at);
  at += integer;
  std::size_t fraction = 0;
  if (at < text.size() && text[at] == '.') {
    ++at;
    fraction = digits_at(text, at);
    at += fraction;
  }
  if (integer == 0 && fraction == 0) {
    return std::nullopt;
  }
  if (at < text.size() && (text[at] == 'e' || text[at] == 'E')) {
    std::size_t exponent = at + 1;
    if (exponent < text.size() &&
        (text[exponent] == '+' || text[exponent] == '-')) {
      ++exponent;
    }
    const std::size_t digits = digits_at(text, exponent);
    if (digits == 0) {
      return std::nullopt;
    }
    at = exponent + digits;
  }
  if (at != text.size()) {
    return std::nullopt;
  }
  // `strtod` wants a terminator, which the view does not promise
  const std::string terminated(text);
  const double value = std::strtod(terminated.c_str(), nullptr);
  if (!std::isfinite(value)) {
    return std::nullopt;
  }
  return value;
}

} // namespace

} // namespace odr::internal::formula

namespace odr::internal {

double formula::Settings::serial(const double days) const {
  if (dialect == Dialect::libreoffice) {
    return days - static_cast<double>(null_date);
  }
  return number_format::serial_from_days(days, epoch);
}

bool formula::approximately_equal(const double a, const double b) {
  // what LibreOffice's `rtl::math::approxEqual` decides: 2^-48 of either
  constexpr double tolerance = 1.0 / (16777216.0 * 16777216.0);
  if (a == b) {
    return true;
  }
  if (a == 0 || b == 0) {
    return false;
  }
  const double difference = std::abs(a - b);
  return std::isfinite(difference) && difference < std::abs(a) * tolerance &&
         difference < std::abs(b) * tolerance;
}

double formula::approximate_add(const double a, const double b) {
  if ((a < 0) != (b < 0) && approximately_equal(a, -b)) {
    return 0;
  }
  return a + b;
}

std::optional<formula::Number>
formula::number_of_text(const std::string_view text) {
  std::string_view trimmed = text;
  while (!trimmed.empty() && trimmed.front() == ' ') {
    trimmed.remove_prefix(1);
  }
  while (!trimmed.empty() && trimmed.back() == ' ') {
    trimmed.remove_suffix(1);
  }
  const bool percent = trimmed.ends_with('%');
  if (const std::optional<double> number = plain_number(
          percent ? trimmed.substr(0, trimmed.size() - 1) : trimmed)) {
    return percent ? *number / 100 : *number;
  }
  const bool ascii = std::ranges::all_of(
      text, [](const char c) { return static_cast<unsigned char>(c) < 0x80; });
  const bool digit = std::ranges::any_of(text, str::is_ascii_digit);
  // LibreOffice reads `TRUE` as 1, and Excel does not
  if (ascii && !digit && !str::equals_ignore_case(trimmed, "TRUE") &&
      !str::equals_ignore_case(trimmed, "FALSE")) {
    return ErrorType::value;
  }
  return std::nullopt;
}

std::optional<std::string> formula::text_of_number(const double number) {
  if (number == 0) {
    return "0";
  }
  if (!std::isfinite(number) || std::abs(number) < 1e-4 ||
      std::abs(number) >= 1e15) {
    return std::nullopt;
  }
  std::string text = fmt::format("{:.15g}", number);
  // a number that rounds up to 1e15 is spelled with an exponent
  if (text.find('e') != std::string::npos) {
    return std::nullopt;
  }
  return text;
}

std::string_view formula::to_string(const ErrorType error) {
  switch (error) {
  case ErrorType::null:
    return "#NULL!";
  case ErrorType::division:
    return "#DIV/0!";
  case ErrorType::value:
    return "#VALUE!";
  case ErrorType::reference:
    return "#REF!";
  case ErrorType::name:
    return "#NAME?";
  case ErrorType::number:
    return "#NUM!";
  case ErrorType::not_available:
    return "#N/A";
  }
  return "#NULL!";
}

std::optional<formula::ErrorType>
formula::error_of_text(const std::string_view text) {
  for (const ErrorType type : error_types) {
    if (to_string(type) == text) {
      return type;
    }
  }
  return std::nullopt;
}

} // namespace odr::internal
