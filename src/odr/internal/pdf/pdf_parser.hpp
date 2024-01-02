#ifndef ODR_INTERNAL_PDF_PARSER_HPP
#define ODR_INTERNAL_PDF_PARSER_HPP

#include <any>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace odr::internal::pdf {

using UnsignedInteger = std::uint64_t;
using Integer = std::int64_t;
using Real = double;
using IntegerOrReal = std::variant<Integer, Real>;
using String = std::string;
using Name = std::string;
using Boolean = bool;
using ObjectReference = std::pair<UnsignedInteger, UnsignedInteger>;
using Object = std::any;
using Array = std::vector<Object>;
using Dictionary = std::unordered_map<Name, Object>;

struct IndirectObject {
  ObjectReference reference;
  Object object;
  bool has_stream{false};
  std::optional<std::string> stream;
};
struct Trailer {
  Dictionary trailer;
};
struct Xref {
  std::unordered_map<std::uint32_t, std::vector<std::string>> table;
};
struct StartXref {
  std::uint32_t start{};
};
struct Eof {};
using Entry = std::any;

class PdfObjectParser {
public:
  explicit PdfObjectParser(std::istream &);

  std::istream &in() const;
  std::streambuf &sb() const;

  void skip_whitespace() const;
  void skip_line() const;
  std::string read_line() const;

  bool peek_number() const;
  UnsignedInteger read_unsigned_integer() const;
  Integer read_integer() const;
  Real read_number() const;
  IntegerOrReal read_integer_or_real() const;

  bool peek_name() const;
  void read_name(std::ostream &) const;
  Name read_name() const;

  bool peek_null() const;
  void read_null() const;

  bool peek_boolean() const;
  Boolean read_boolean() const;

  bool peek_string() const;
  void read_string(std::ostream &) const;
  String read_string() const;

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

class PdfFileParser {
public:
  explicit PdfFileParser(std::istream &);

  IndirectObject read_indirect_object(std::optional<std::string> head) const;
  Trailer read_trailer(std::optional<std::string> head) const;
  Xref read_xref(std::optional<std::string> head) const;
  StartXref read_startxref(std::optional<std::string> head) const;

  void read_header() const;
  Entry read_entry() const;

private:
  PdfObjectParser m_parser;
};

} // namespace odr::internal::pdf

#endif // ODR_INTERNAL_PDF_PARSER_HPP
