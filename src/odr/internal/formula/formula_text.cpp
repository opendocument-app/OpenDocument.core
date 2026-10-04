#include <odr/internal/formula/formula_text.hpp>

#include <odr/internal/util/string_util.hpp>

#include <algorithm>
#include <cstdint>
#include <tuple>

namespace odr::internal::formula {

namespace {

/// Whether @p c has no case to map: a mark, a digit, a symbol, punctuation
/// and the scripts without case.
bool is_caseless(const char16_t c) {
  return c < u'A' || (c > u'Z' && c < u'a') || (c > u'z' && c < 0x80) ||
         (c >= 0xA0 && c <= 0xBF && c != 0xB5) || c == 0xD7 || c == 0xF7 ||
         (c >= 0x2000 && c <= 0x206F) || (c >= 0x20A0 && c <= 0x20CF) ||
         (c >= 0x3000 && c <= 0x9FFF);
}

/// The other case of @p c within one of the ranges of Latin Extended-A
/// whose letters alternate, upper case on an odd code where @p upper_odd.
std::optional<char16_t> alternating(const char16_t c, const char16_t first,
                                    const char16_t last, const bool upper_odd,
                                    const bool to_upper) {
  if (c < first || c > last) {
    return std::nullopt;
  }
  const bool is_upper = (c % 2 == 1) == upper_odd;
  if (is_upper == to_upper) {
    return c;
  }
  return static_cast<char16_t>(to_upper ? c - 1 : c + 1);
}

std::optional<char16_t> mapped(const char16_t c, const bool to_upper) {
  if (is_caseless(c)) {
    return c;
  }
  if (c <= 0x7F) {
    const bool is_upper = c <= u'Z';
    if (is_upper == to_upper) {
      return c;
    }
    return static_cast<char16_t>(to_upper ? c - 32 : c + 32);
  }
  if (c >= 0xC0 && c <= 0xDE) {
    return to_upper ? c : static_cast<char16_t>(c + 32);
  }
  if (c >= 0xE0 && c <= 0xFE) {
    return to_upper ? static_cast<char16_t>(c - 32) : c;
  }
  if (c == 0xFF) {
    return to_upper ? u'Ÿ' : c;
  }
  if (c == 0x178) {
    return to_upper ? c : u'ÿ';
  }
  // the letters whose case the applications map apart, or which have none
  // of one length: `İ`, `ı`, `ĸ`, `ŉ`, `ſ`
  if (c == 0x130 || c == 0x131 || c == 0x138 || c == 0x149 || c == 0x17F) {
    return std::nullopt;
  }
  for (const auto &[first, last, parity] :
       {std::tuple<char16_t, char16_t, bool>{0x100, 0x137, false},
        {0x139, 0x148, true},
        {0x14A, 0x177, false},
        {0x179, 0x17E, true}}) {
    if (const std::optional<char16_t> other =
            alternating(c, first, last, parity, to_upper)) {
      return other;
    }
  }
  return std::nullopt;
}

std::optional<std::u16string> mapped(const std::u16string &text,
                                     const bool to_upper) {
  std::u16string result;
  result.reserve(text.size());
  for (const char16_t c : text) {
    const std::optional<char16_t> other = mapped(c, to_upper);
    if (!other.has_value()) {
      return std::nullopt;
    }
    result += *other;
  }
  return result;
}

} // namespace

} // namespace odr::internal::formula

namespace odr::internal {

std::optional<std::u16string> formula::utf16_of(const std::string_view text) {
  std::u16string result = util::string::string_to_u16string(text);
  if (std::ranges::any_of(result, [](const char16_t c) {
        return c >= 0xD800 && c <= 0xDFFF;
      })) {
    return std::nullopt;
  }
  return result;
}

std::size_t formula::utf16_length(const std::string_view text) {
  std::size_t result = 0;
  for (const char c : text) {
    const auto byte = static_cast<std::uint8_t>(c);
    // a lead byte starts a character, and one of four bytes takes two units
    if ((byte & 0xC0) != 0x80) {
      result += byte >= 0xF0 ? 2 : 1;
    }
  }
  return result;
}

std::string formula::utf8_of(const std::u16string &text) {
  return util::string::u16string_to_string(text);
}

std::optional<std::u16string> formula::to_upper(const std::u16string &text) {
  return mapped(text, true);
}

std::optional<std::u16string> formula::to_lower(const std::u16string &text) {
  return mapped(text, false);
}

std::optional<std::string> formula::folded(const std::string_view text) {
  const std::optional<std::u16string> units = utf16_of(text);
  if (!units.has_value()) {
    return std::nullopt;
  }
  const std::optional<std::u16string> lower = to_lower(*units);
  if (!lower.has_value()) {
    return std::nullopt;
  }
  return utf8_of(*lower);
}

} // namespace odr::internal
