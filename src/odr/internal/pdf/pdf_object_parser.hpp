#ifndef ODR_INTERNAL_PDF_OBJECT_PARSER_HPP
#define ODR_INTERNAL_PDF_OBJECT_PARSER_HPP

#include <odr/internal/pdf/pdf_object.hpp>

#include <istream>
#include <variant>

namespace odr::internal::pdf {

class ObjectParser {
public:
  explicit ObjectParser(std::istream &);

  std::istream &in() const;
  std::streambuf &sb() const;

  void skip_whitespace() const;
  void skip_line() const;
  std::string read_line(bool inclusive = false) const;

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
