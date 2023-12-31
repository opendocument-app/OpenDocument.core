#ifndef ODR_INTERNAL_PDF_PARSER_HPP
#define ODR_INTERNAL_PDF_PARSER_HPP

#include <iosfwd>
#include <optional>
#include <string>

namespace odr::internal::pdf {

namespace parser {

void read_header(std::istream &);

void read_entry(std::istream &);

void skip_whitespace(std::istream &);

void read_object(std::istream &, std::optional<std::string> head);

void read_xref(std::istream &, std::optional<std::string> head);
void read_startxref(std::istream &, std::optional<std::string> head);

bool peek_integer(std::istream &);
std::int64_t read_integer(std::istream &);
std::optional<std::int64_t> try_read_integer(std::istream &);

} // namespace parser

} // namespace odr::internal::pdf

#endif // ODR_INTERNAL_PDF_PARSER_HPP
