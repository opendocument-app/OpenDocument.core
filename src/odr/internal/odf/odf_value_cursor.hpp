#pragma once

#include <cstddef>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>

namespace odr::internal::odf {

/// A cursor over one of the small languages an odf attribute is written in.
/// Reads are bounded by what remains, which carries no terminator.
class ValueCursor {
public:
  explicit ValueCursor(const std::string_view input) : m_rest{input} {}

  [[nodiscard]] bool empty() const { return m_rest.empty(); }

  /// The next character, or `\0` where the input ended.
  [[nodiscard]] char peek() const {
    return m_rest.empty() ? '\0' : m_rest.front();
  }

  /// The next character, consumed.
  char take() {
    const char c = peek();
    if (!m_rest.empty()) {
      m_rest.remove_prefix(1);
    }
    return c;
  }

  /// Whitespace only: a comma separates the arguments of a formula.
  void skip_space() {
    while (is_space(peek())) {
      m_rest.remove_prefix(1);
    }
  }

  /// Whitespace and the commas a coordinate list may be written with.
  void skip_separators() {
    while (is_space(peek()) || peek() == ',') {
      m_rest.remove_prefix(1);
    }
  }

  /// Only spaces are skipped ahead of @p c: a comma is an argument separator
  /// where a formula is concerned, not filler.
  [[nodiscard]] bool consume(const char c) {
    skip_space();
    if (peek() != c) {
      return false;
    }
    m_rest.remove_prefix(1);
    return true;
  }

  /// The leading run of characters @p accept admits, left in place.
  [[nodiscard]] std::string_view peek_while(bool (*accept)(char)) const {
    std::size_t length = 0;
    while (length < m_rest.size() && accept(m_rest[length])) {
      ++length;
    }
    return m_rest.substr(0, length);
  }

  /// The same run, consumed.
  [[nodiscard]] std::string_view take_while(bool (*accept)(char)) {
    const std::string_view taken = peek_while(accept);
    m_rest.remove_prefix(taken.size());
    return taken;
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
    m_rest.remove_prefix(static_cast<std::size_t>(end - number.c_str()));
    return value;
  }

  [[nodiscard]] bool starts_number() const {
    const char c = peek();
    return c == '-' || c == '+' || c == '.' || is_digit(c);
  }

  static bool is_letter(const char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
  }
  static bool is_digit(const char c) { return c >= '0' && c <= '9'; }
  static bool is_letter_or_digit(const char c) {
    return is_letter(c) || is_digit(c);
  }

private:
  static bool is_space(const char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
  }
  /// A superset of a number's characters, to bound the run `std::strtod` reads.
  static bool is_number_char(const char c) {
    return is_digit(c) || c == '+' || c == '-' || c == '.' || c == 'e' ||
           c == 'E';
  }

  std::string_view m_rest;
};

} // namespace odr::internal::odf
