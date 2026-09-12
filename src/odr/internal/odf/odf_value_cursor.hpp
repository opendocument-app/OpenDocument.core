#pragma once

#include <odr/internal/common/text_cursor.hpp>
#include <odr/internal/util/string_util.hpp>

#include <cstddef>
#include <cstdlib>
#include <optional>
#include <string>

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

  /// `std::strtod` wants a terminator, which the view does not promise, so the
  /// run it bounds is copied out.
  [[nodiscard]] std::optional<double> read_number() {
    skip_separators();
    const std::string number(peek_while(is_number_char));
    char *end = nullptr;
    const double value = std::strtod(number.c_str(), &end);
    if (end == number.c_str()) {
      return {};
    }
    // `strtod` may stop short of the run, on a trailing `e` say
    advance(static_cast<std::size_t>(end - number.c_str()));
    return value;
  }

  [[nodiscard]] bool starts_number() const {
    const char c = peek();
    return c == '-' || c == '+' || c == '.' || str::is_ascii_digit(c);
  }

private:
  /// A superset of a number's characters, to bound the run `std::strtod` reads.
  static bool is_number_char(const char c) {
    return str::is_ascii_digit(c) || c == '+' || c == '-' || c == '.' ||
           c == 'e' || c == 'E';
  }
};

} // namespace odr::internal::odf
