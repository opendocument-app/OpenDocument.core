#ifndef ODR_INTERNAL_PDF_PARSER_HPP
#define ODR_INTERNAL_PDF_PARSER_HPP

#include <any>
#include <iosfwd>
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

namespace parser {

void read_header(std::istream &);

void read_entry(std::istream &);

void read_indirect_object(std::istream &, std::optional<std::string> head);

void read_xref(std::istream &, std::optional<std::string> head);
void read_startxref(std::istream &, std::optional<std::string> head);

void skip_whitespace(std::istream &);

bool peek_number(std::istream &);
UnsignedInteger read_unsigned_integer(std::istream &);
Integer read_integer(std::istream &);
Real read_number(std::istream &);
IntegerOrReal read_integer_or_real(std::istream &);

bool peek_name(std::istream &);
void read_name(std::istream &, std::ostream &);
Name read_name(std::istream &);

bool peek_null(std::istream &);
void read_null(std::istream &);

bool peek_boolean(std::istream &);
Boolean read_boolean(std::istream &);

bool peek_string(std::istream &);
void read_string(std::istream &, std::ostream &);
String read_string(std::istream &);

bool peek_array(std::istream &);
Array read_array(std::istream &);

bool peek_dictionary(std::istream &);
Dictionary read_dictionary(std::istream &);

Object read_object(std::istream &);

ObjectReference read_object_reference(std::istream &);

} // namespace parser

} // namespace odr::internal::pdf

#endif // ODR_INTERNAL_PDF_PARSER_HPP
