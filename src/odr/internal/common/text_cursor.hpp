#pragma once

#include <odr/internal/util/string_util.hpp>

#include <cstddef>
#include <string_view>

namespace odr::internal {

/// A cursor over a small language written into one string — an odf attribute,
/// a formula. Reads are bounded by what remains, which carries no terminator.
class TextCursor {
public:
  explicit TextCursor(const std::string_view input) : m_rest{input} {}

  [[nodiscard]] bool empty() const { return m_rest.empty(); }

  /// What the cursor has not read, which @ref seek takes back.
  [[nodiscard]] std::string_view rest() const { return m_rest; }

  /// The character @p ahead of the cursor, or `\0` past the end.
  [[nodiscard]] char peek(const std::size_t ahead = 0) const {
    return ahead < m_rest.size() ? m_rest[ahead] : '\0';
  }

  /// The next character, consumed.
  char take() {
    const char c = peek();
    if (!m_rest.empty()) {
      advance(1);
    }
    return c;
  }

  void advance(const std::size_t count) { m_rest.remove_prefix(count); }

  /// Back to a view @ref rest answered earlier, undoing what was read since.
  void seek(const std::string_view at) { m_rest = at; }

  void skip_whitespace() {
    while (util::string::is_ascii_whitespace(peek())) {
      advance(1);
    }
  }

  /// Whitespace ahead of it is filler; the text itself is the token.
  [[nodiscard]] bool consume(const char c) {
    skip_whitespace();
    if (peek() != c) {
      return false;
    }
    advance(1);
    return true;
  }

  [[nodiscard]] bool consume(const std::string_view text) {
    skip_whitespace();
    if (!m_rest.starts_with(text)) {
      return false;
    }
    advance(text.size());
    return true;
  }

  /// The leading run of characters @p accept admits, left in place.
  [[nodiscard]] std::string_view
  peek_while(const util::string::CharPredicate accept) const {
    std::size_t length = 0;
    while (length < m_rest.size() && accept(m_rest[length])) {
      ++length;
    }
    return m_rest.substr(0, length);
  }

  /// The same run, consumed.
  [[nodiscard]] std::string_view
  take_while(const util::string::CharPredicate accept) {
    const std::string_view taken = peek_while(accept);
    advance(taken.size());
    return taken;
  }

private:
  std::string_view m_rest;
};

} // namespace odr::internal
