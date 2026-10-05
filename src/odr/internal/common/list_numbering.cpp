#include <odr/internal/common/list_numbering.hpp>

#include <algorithm>
#include <array>
#include <limits>
#include <ranges>
#include <stdexcept>

namespace odr::internal {

namespace {

std::string format_decimal(const std::uint32_t number) {
  return std::to_string(number);
}

std::string format_letter(std::uint32_t number, const char base) {
  std::string result;
  for (; number > 0; number = (number - 1) / 26) {
    result.push_back(static_cast<char>(base + (number - 1) % 26));
  }
  std::ranges::reverse(result);
  return result;
}

std::string format_roman(std::uint32_t number, const bool upper) {
  static constexpr std::array<std::uint32_t, 13> values{
      1000, 900, 500, 400, 100, 90, 50, 40, 10, 9, 5, 4, 1};
  static constexpr std::array<const char *, 13> upper_symbols{
      "M", "CM", "D", "CD", "C", "XC", "L", "XL", "X", "IX", "V", "IV", "I"};
  static constexpr std::array<const char *, 13> lower_symbols{
      "m", "cm", "d", "cd", "c", "xc", "l", "xl", "x", "ix", "v", "iv", "i"};

  // Roman has no zero and the additive notation runs out of thousands marks.
  if (number == 0 || number > 3999) {
    return format_decimal(number);
  }

  const auto &symbols = upper ? upper_symbols : lower_symbols;

  std::string result;
  for (std::size_t i = 0; i < values.size(); ++i) {
    for (; number >= values[i]; number -= values[i]) {
      result.append(symbols[i]);
    }
  }
  return result;
}

} // namespace

} // namespace odr::internal

namespace odr {

std::string internal::format_list_number(const ListNumberFormat format,
                                         const std::uint32_t number) {
  switch (format) {
  case ListNumberFormat::decimal:
    return format_decimal(number);
  case ListNumberFormat::decimal_zero:
    return number < 10 ? "0" + format_decimal(number) : format_decimal(number);
  case ListNumberFormat::letter_lower:
    return format_letter(number, 'a');
  case ListNumberFormat::letter_upper:
    return format_letter(number, 'A');
  case ListNumberFormat::roman_lower:
    return format_roman(number, false);
  case ListNumberFormat::roman_upper:
    return format_roman(number, true);
  case ListNumberFormat::none:
  case ListNumberFormat::bullet:
  default:
    return "";
  }
}

} // namespace odr

namespace odr::internal {

std::string ListCounter::advance(const std::uint32_t level,
                                 const ListLevel &list_level) {
  grow_(level);

  auto &number = m_numbers[level];
  if (number == std::numeric_limits<std::uint32_t>::max()) {
    throw std::overflow_error("List counter overflow");
  }
  number = number ? *number + 1 : list_level.start;
  m_formats[level] = list_level.format;
  std::ranges::fill(m_numbers |
                        std::views::drop(static_cast<std::size_t>(level) + 1),
                    std::nullopt);

  if (list_level.format == ListNumberFormat::bullet) {
    return list_level.label;
  }

  std::string result;
  const std::string &label = list_level.label;
  for (std::size_t i = 0; i < label.size();) {
    const char c = label[i];
    if (c != '%' || i + 1 >= label.size() || label[i + 1] < '1' ||
        label[i + 1] > '9') {
      result.push_back(c);
      ++i;
      continue;
    }
    const auto placeholder = static_cast<std::uint32_t>(label[i + 1] - '1');
    if (placeholder < m_numbers.size()) {
      result.append(format_list_number(m_formats[placeholder],
                                       m_numbers[placeholder].value_or(1)));
    }
    i += 2;
  }
  return result;
}

std::uint32_t ListCounter::number(const std::uint32_t level) const {
  return level < m_numbers.size() ? m_numbers[level].value_or(0) : 0;
}

void ListCounter::restart(const std::uint32_t level,
                          const std::uint32_t number) {
  grow_(level);
  m_numbers[level] = number == 0 ? std::nullopt : std::optional(number);
}

void ListCounter::grow_(const std::uint32_t level) {
  if (level == std::numeric_limits<std::uint32_t>::max()) {
    throw std::overflow_error("List level overflow");
  }
  if (level >= m_numbers.size()) {
    m_numbers.resize(static_cast<std::size_t>(level) + 1);
    m_formats.resize(level + 1, ListNumberFormat::decimal);
  }
}

} // namespace odr::internal
