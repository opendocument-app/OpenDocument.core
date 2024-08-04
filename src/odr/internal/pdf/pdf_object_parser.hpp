#ifndef ODR_INTERNAL_PDF_OBJECT_PARSER_HPP
#define ODR_INTERNAL_PDF_OBJECT_PARSER_HPP

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

  std::istream &in() const;
  std::streambuf &sb() const;

  int_type geti() const;
  char_type getc() const;
  char_type bumpc() const;
  template <std::uint32_t N> std::array<char, N> bumpnc() const {
    std::array<char, N> result;
    if (sb().sgetn(result.data(), result.size()) != result.size()) {
      throw std::runtime_error("unexpected stream exhaust");
    }
    return result;
  }
  std::string bumpnc(std::size_t n) const;
  void ungetc() const;

  static int_type octet_char_to_int(char_type c);
  static int_type hex_char_to_int(char_type c);
  static char_type two_hex_to_char(char_type first, char_type second);
  static char_type three_octet_to_char(char_type first, char_type second,
                                       char_type third);

  static bool is_whitespace(char c);
  bool peek_whitespace() const;
  void skip_whitespace() const;
  void skip_line() const;
  std::string read_line(bool inclusive = false) const;
  void expect_characters(const std::string &string) const;

  bool peek_number() const;
  UnsignedInteger read_unsigned_integer() const;
  Integer read_integer() const;
  Real read_number() const;
  std::variant<Integer, Real> read_integer_or_real() const;

  bool peek_name() const;
  void read_name(std::ostream &) const;
  Name read_name() const;

  bool peek_null() const;
  void read_null() const;

  bool peek_boolean() const;
  Boolean read_boolean() const;

  bool peek_string() const;
  std::variant<StandardString, HexString> read_string() const;

  bool peek_array() const;
  Array read_array() const;

  bool peek_dictionary() const;
  Dictionary read_dictionary() const;

  Object read_object() const;

  ObjectReference read_object_reference() const;

private:
  std::istream *m_in;
  std::istream::sentry m_se;
  std::streambuf *m_sb;
};

} // namespace odr::internal::pdf

#endif // ODR_INTERNAL_PDF_OBJECT_PARSER_HPP
