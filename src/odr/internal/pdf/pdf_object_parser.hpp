#pragma once

#include <odr/internal/pdf/pdf_object.hpp>

#include <array>
#include <istream>
#include <stdexcept>
#include <string_view>
#include <variant>

namespace odr::internal::pdf {

class ObjectParser {
public:
  using char_type = std::streambuf::char_type;
  using int_type = std::streambuf::int_type;
  static constexpr int_type eof = std::streambuf::traits_type::eof();
  using pos_type = std::streambuf::pos_type;

  explicit ObjectParser(std::istream &in);

  [[nodiscard]] std::istream &in();
  [[nodiscard]] std::streambuf &sb();

  int_type geti();
  char_type getc();
  char_type bumpc();
  template <std::uint32_t N> std::array<char, N> bumpnc() {
    std::array<char, N> result;
    if (sb().sgetn(result.data(), result.size()) != result.size()) {
      throw std::runtime_error("unexpected stream exhaust");
    }
    return result;
  }
  std::string bumpnc(std::size_t n);
  void ungetc();

  static std::uint8_t octet_char_to_int(char_type c);
  static std::uint8_t hex_char_to_int(char_type c);
  static char_type two_hex_to_char(char_type first, char_type second);
  static char_type three_octet_to_char(char_type first, char_type second,
                                       char_type third);

  static bool is_whitespace(char c);
  [[nodiscard]] bool peek_whitespace();
  void skip_whitespace();
  void skip_line();
  std::string read_line(bool inclusive = false);
  /// Advance the cursor just past the next occurrence of `marker`. Returns true
  /// if it was found; on false the stream has been consumed to eof. Operates on
  /// raw bytes, so the marker may straddle line breaks.
  bool skip_past(std::string_view marker);
  void expect_characters(const std::string &string);

  [[nodiscard]] bool peek_number();
  [[nodiscard]] bool peek_unsigned_integer();
  [[nodiscard]] std::pair<UnsignedInteger, std::uint32_t>
  read_unsigned_integer_and_count();
  [[nodiscard]] UnsignedInteger read_unsigned_integer();
  [[nodiscard]] Integer read_integer();
  [[nodiscard]] Real read_number();
  [[nodiscard]] std::variant<Integer, Real> read_integer_or_real();

  [[nodiscard]] bool peek_name();
  void read_name(std::ostream &);
  [[nodiscard]] Name read_name();

  [[nodiscard]] bool peek_null();
  void read_null();

  [[nodiscard]] bool peek_boolean();
  [[nodiscard]] Boolean read_boolean();

  [[nodiscard]] bool peek_string();
  [[nodiscard]] std::variant<StandardString, HexString> read_string();

  [[nodiscard]] bool peek_array();
  [[nodiscard]] Array read_array();

  [[nodiscard]] bool peek_dictionary();
  [[nodiscard]] Dictionary read_dictionary();

  /// Read one *self-delimiting* object: null, boolean, number, name, string,
  /// array, or dictionary. Arrays and dictionaries may contain indirect
  /// references (assembled while parsing them), but a bare top-level reference
  /// is **not** returned as such — `n g R` is not recognizable from its first
  /// token (it looks like the integer `n` until the `R` two tokens later, and
  /// in an array `[1 2 3]` a leading integer is ambiguous with a reference
  /// start). So a leading object number comes back as a plain integer; the
  /// enclosing context folds it into a reference — `read_array` on seeing the
  /// `R` token, dictionary values and indirect-object bodies via
  /// `promote_indirect_reference` — or use `read_object_reference` where a
  /// reference is required outright.
  [[nodiscard]] Object read_object();

  /// With the cursor just past a freshly-read `value` (trailing whitespace
  /// skipped), fold `value` in place into an `n g R` indirect reference if a
  /// `g R` tail follows; otherwise leave `value` untouched. Only valid where a
  /// value cannot be followed by another bare number — dictionary values and
  /// indirect-object bodies — not array elements.
  void promote_indirect_reference(Object &value);

  [[nodiscard]] ObjectReference read_object_reference();

private:
  std::istream *m_in{nullptr};
  std::istream::sentry m_se;
  std::streambuf *m_sb{nullptr};
};

} // namespace odr::internal::pdf
