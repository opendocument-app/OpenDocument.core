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

  static std::uint8_t hex_char_to_int(char_type c);
  static char_type two_hex_to_char(char_type first, char_type second);

  /// PostScript white space, which PDF shares (7.2.2).
  static bool is_whitespace(char c);
  /// The delimiters of 7.2.2, each of which opens a token of its own.
  static bool is_delimiter(char c);
  [[nodiscard]] bool peek_whitespace();
  void skip_whitespace();
  /// Skip white space and comments (7.2.4); do not call between file entries.
  void skip_whitespace_and_comments();
  void skip_line();
  std::string read_line(bool inclusive = false);
  /// Consume through the next raw-byte marker; return false at EOF.
  bool skip_past(std::string_view marker);
  /// Require an exact, complete keyword (7.2.2).
  void expect_keyword(const std::string &keyword);
  void expect_characters(const std::string &string);
  /// A bareword such as `endobj` or `Tj`, up to the next white space or
  /// delimiter (7.2.2). Empty if the cursor is at one or at eof.
  [[nodiscard]] std::string read_keyword();

  [[nodiscard]] bool peek_number();
  [[nodiscard]] bool peek_unsigned_integer();
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

  /// Read one object. Enclosing containers fold `n g R` into a reference;
  /// use read_object_reference when a reference is required.
  [[nodiscard]] Object read_object();

  /// Fold a following `g R` into value, with leading whitespace skipped.
  /// Valid only where another bare number cannot follow, never in arrays.
  void promote_indirect_reference(Object &value);

  [[nodiscard]] ObjectReference read_object_reference();

private:
  /// Bound recursive composite parsing to protect the C++ stack.
  static constexpr std::uint32_t max_nesting_depth = 256;

  /// RAII depth counter around one composite read; `throw`s past the limit.
  class NestingGuard {
  public:
    explicit NestingGuard(ObjectParser &parser);
    ~NestingGuard();
    NestingGuard(const NestingGuard &) = delete;
    NestingGuard &operator=(const NestingGuard &) = delete;

  private:
    ObjectParser *m_parser;
  };

  std::istream *m_in{nullptr};
  std::streambuf *m_sb{nullptr};
  std::uint32_t m_depth{0};
};

} // namespace odr::internal::pdf
