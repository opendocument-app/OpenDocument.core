#pragma once

#include <odr/internal/common/text_cursor.hpp>
#include <odr/internal/util/number_util.hpp>
#include <odr/internal/util/string_util.hpp>

#include <cstddef>
#include <optional>

namespace odr::internal::odf {

namespace str = util::string;

/// A cursor over one of the small languages an odf attribute is written in.
class ValueCursor : public TextCursor {
public:
  using TextCursor::TextCursor;

  /// Whitespace and the commas a coordinate list may be written with. @ref
  /// consume leaves a comma, which a formula separates its arguments with.
  void skip_separators() {
    while (str::is_ascii_whitespace(peek()) || peek() == ',') {
      advance(1);
    }
  }

  /// Reads one decimal token, leaving following operators and commands.
  [[nodiscard]] std::optional<double> read_number() {
    skip_separators();
    std::size_t end = peek() == '+' || peek() == '-' ? 1 : 0;
    const auto digits = [&] {
      const std::size_t begin = end;
      while (str::is_ascii_digit(peek(end))) {
        ++end;
      }
      return end != begin;
    };
    bool has_digits = digits();
    if (peek(end) == '.') {
      ++end;
      has_digits = digits() || has_digits;
    }
    if (!has_digits) {
      return {};
    }
    if (peek(end) == 'e' || peek(end) == 'E') {
      const std::size_t exponent = end++;
      if (peek(end) == '+' || peek(end) == '-') {
        ++end;
      }
      if (!digits()) {
        end = exponent;
      }
    }
    const std::optional<double> value =
        util::number::parse(rest().substr(0, end));
    if (value) {
      advance(end);
    }
    return value;
  }

  [[nodiscard]] bool starts_number() const {
    const char c = peek();
    return c == '-' || c == '+' || c == '.' || str::is_ascii_digit(c);
  }
};

} // namespace odr::internal::odf
