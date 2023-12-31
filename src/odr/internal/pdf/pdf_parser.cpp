#include <odr/internal/pdf/pdf_parser.hpp>

#include <odr/internal/util/stream_util.hpp>
#include <odr/internal/util/string_util.hpp>

#include <iostream>

namespace odr::internal::pdf {

using char_type = std::streambuf::char_type;
using int_type = std::streambuf::int_type;
static constexpr int_type eof = std::streambuf::traits_type::eof();

void parser::read_header(std::istream &in) {
  std::string header1 = util::stream::read_until(in, '\n');
  std::string header2 = util::stream::read_until(in, '\n');

  std::cout << header1 << std::endl;
  std::cout << header2 << std::endl;
}

void parser::read_entry(std::istream &in) {
  skip_whitespace(in);
  std::string entry_header = util::stream::read_until(in, '\n');

  std::cout << entry_header << std::endl;

  if (util::string::ends_with(entry_header, "obj")) {
    std::cout << ">> this is an object" << std::endl;
    read_object(in, std::move(entry_header));
    return;
  }

  if (entry_header == "xref") {
    std::cout << ">> this is an xref" << std::endl;
    read_xref(in, std::move(entry_header));
    return;
  }

  if (entry_header == "trailer") {
    std::cout << ">> this is a trailer" << std::endl;
    return;
  }

  if (entry_header == "startxref") {
    std::cout << ">> this is a startxref" << std::endl;
    read_startxref(in, std::move(entry_header));
    return;
  }

  if (entry_header == "%%EOF") {
    std::cout << ">> this is the end" << std::endl;
    return;
  }

  std::cout << ">> this is unknown" << std::endl;
}

void parser::skip_whitespace(std::istream &in) {
  std::istream::sentry se(in, true);
  std::streambuf *sb = in.rdbuf();

  while (true) {
    int_type c = sb->sgetc();
    switch (c) {
    case ' ':
    case '\n':
      sb->sbumpc();
      break;
    case eof:
      in.setstate(std::ios::eofbit);
    default:
      return;
    }
  }
}

void parser::read_object(std::istream &in, std::optional<std::string> head) {
  if (!head) {
    head = util::stream::read_until(in, '\n');
  }

  while (true) {
    if (in.eof()) {
      throw std::runtime_error("input exhausted");
    }
    std::string line = util::stream::read_until(in, '\n');
    if (line == "endobj") {
      break;
    }
  }
}

void parser::read_xref(std::istream &in, std::optional<std::string> head) {
  if (!head) {
    head = util::stream::read_until(in, '\n');
  }

  while (true) {
    if (in.eof()) {
      throw std::runtime_error("input exhausted");
    }
    if (!peek_integer(in)) {
      return;
    }
    std::uint32_t subsection_id = read_integer(in);
    skip_whitespace(in);
    std::uint32_t subsection_size = read_integer(in);
    util::stream::read_until(in, '\n');
    for (std::uint32_t i = 0; i < subsection_size; ++i) {
      std::string subsection_entry = util::stream::read_until(in, '\n');
    }
  }
}

void parser::read_startxref(std::istream &in, std::optional<std::string> head) {
  if (!head) {
    head = util::stream::read_until(in, '\n');
  }

  std::uint32_t startxref = peek_integer(in);
  util::stream::read_until(in, '\n');
}

bool parser::peek_integer(std::istream &in) {
  std::istream::sentry se(in, true);
  std::streambuf *sb = in.rdbuf();

  int_type c = sb->sgetc();
  return c != eof && (c == '+' || c == '-' || std::isdigit(c));
}

std::int64_t parser::read_integer(std::istream &in) {
  std::istream::sentry se(in, true);
  std::streambuf *sb = in.rdbuf();

  std::int64_t result = 0;
  std::int8_t sign = 1;

  if (sb->sgetc() == '+') {
    sign = +1;
    sb->sbumpc();
  }
  if (sb->sgetc() == '-') {
    sign = -1;
    sb->sbumpc();
  }

  while (true) {
    int_type c = sb->sgetc();
    if (c == eof) {
      in.setstate(std::ios::eofbit);
      return sign * result;
    }
    if (!std::isdigit(c)) {
      return sign * result;
    }
    result = result * 10 + (c - '0');
    sb->sbumpc();
  }
}

std::optional<std::int64_t> parser::try_read_integer(std::istream &in) {
  if (peek_integer(in)) {
    return std::nullopt;
  }
  return read_integer(in);
}

} // namespace odr::internal::pdf
