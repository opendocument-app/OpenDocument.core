#pragma once

#include <odr/internal/pdf/pdf_object.hpp>

#include <array>
#include <istream>
#include <stdexcept>
#include <variant>

namespace odr::internal::pdf {

class ObjectParser {
public:
  using char_type = std::streambuf::char_type;
  using int_type = std::streambuf::int_type;
  static constexpr int_type eof = std::streambuf::traits_type::eof();
  using pos_type = std::streambuf::pos_type;

  explicit ObjectParser(std::istream &);

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

  static int_type octet_char_to_int(char_type c);
  static int_type hex_char_to_int(char_type c);
  static char_type two_hex_to_char(char_type first, char_type second);
  static char_type three_octet_to_char(char_type first, char_type second,
                                       char_type third);

  static bool is_whitespace(char c);
  [[nodiscard]] bool peek_whitespace();
  void skip_whitespace();
  void skip_line();
  std::string read_line(bool inclusive = false);
  void expect_characters(const std::string &string);

  [[nodiscard]] bool peek_number();
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

  [[nodiscard]] Object read_object();

  [[nodiscard]] ObjectReference read_object_reference();

private:
  std::istream *m_in{nullptr};
  std::istream::sentry m_se;
  std::streambuf *m_sb{nullptr};
};

} // namespace odr::internal::pdf
