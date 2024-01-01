#include <odr/internal/pdf/pdf_parser.hpp>

#include <odr/internal/util/stream_util.hpp>
#include <odr/internal/util/string_util.hpp>

#include <cmath>
#include <cstring>
#include <iostream>
#include <sstream>

namespace odr::internal::pdf {

using char_type = std::streambuf::char_type;
using int_type = std::streambuf::int_type;
static constexpr int_type eof = std::streambuf::traits_type::eof();

namespace {

int_type hex_char_to_int(char_type c) {
  if (c >= 'a') {
    return 10 + (c - 'a');
  }
  if (c >= 'A') {
    return 10 + (c - 'A');
  }
  return c - '0';
}

char_type two_hex_to_char(char_type first, char_type second) {
  return hex_char_to_int(first) * 16 + hex_char_to_int(second);
}

} // namespace

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
    read_indirect_object(in, std::move(entry_header));
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

void parser::read_indirect_object(std::istream &in,
                                  std::optional<std::string> head) {
  if (!head) {
    head = util::stream::read_until(in, '\n');
  }

  Object object = read_object(in);
  util::stream::read_until(in, '\n');
  if (util::stream::read_until(in, '\n') != "endobj") {
    throw std::runtime_error("expected endobj");
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
    if (!peek_number(in)) {
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

  std::uint32_t startxref = peek_number(in);
  util::stream::read_until(in, '\n');
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

bool parser::peek_number(std::istream &in) {
  std::istream::sentry se(in, true);
  std::streambuf *sb = in.rdbuf();

  int_type c = sb->sgetc();
  return c != eof && (c == '+' || c == '-' || c == '.' || std::isdigit(c));
}

UnsignedInteger parser::read_unsigned_integer(std::istream &in) {
  std::istream::sentry se(in, true);
  std::streambuf *sb = in.rdbuf();

  UnsignedInteger result = 0;

  while (true) {
    int_type c = sb->sgetc();
    if (c == eof) {
      in.setstate(std::ios::eofbit);
      return result;
    }
    if (!std::isdigit(c)) {
      return result;
    }
    result = result * 10 + (c - '0');
    sb->sbumpc();
  }
}

Integer parser::read_integer(std::istream &in) {
  std::istream::sentry se(in, true);
  std::streambuf *sb = in.rdbuf();

  std::int8_t sign = 1;

  if (sb->sgetc() == '+') {
    sign = +1;
    sb->sbumpc();
  }
  if (sb->sgetc() == '-') {
    sign = -1;
    sb->sbumpc();
  }

  return sign * read_unsigned_integer(in);
}

Real parser::read_number(std::istream &in) {
  return std::visit([](auto v) -> Real { return v; }, read_integer_or_real(in));
}

IntegerOrReal parser::read_integer_or_real(std::istream &in) {
  std::istream::sentry se(in, true);
  std::streambuf *sb = in.rdbuf();

  Integer i = 0;

  int_type c = sb->sgetc();
  if (c != '.') {
    i = read_integer(in);
    c = sb->sgetc();
    if (c != '.') {
      return i;
    }
  }
  sb->sbumpc();

  std::streamsize begin = in.gcount();
  UnsignedInteger i2 = read_unsigned_integer(in);
  std::streamsize end = in.gcount();

  return i + i2 * std::pow(10.0, begin - end);
}

bool parser::peek_name(std::istream &in) {
  std::istream::sentry se(in, true);
  std::streambuf *sb = in.rdbuf();

  int_type c = sb->sgetc();
  return c != eof && c == '/';
}

void parser::read_name(std::istream &in, std::ostream &out) {
  std::istream::sentry se(in, true);
  std::streambuf *sb = in.rdbuf();

  if (int_type c = sb->sbumpc(); c != '/') {
    throw std::runtime_error("not a name");
  }

  while (true) {
    int_type c = sb->sgetc();

    if (c == eof) {
      in.setstate(std::ios::eofbit);
      return;
    }
    if (c < 0x21 || c > 0x7e || c == '/' || c == '%' || c == '(' || c == ')' ||
        c == '<' || c == '>' || c == '[' || c == ']' || c == '{' || c == '}') {
      return;
    }

    if (c == '#') {
      sb->sbumpc();
      char hex[2];
      if (sb->sgetn(hex, 2) != 2) {
        throw std::runtime_error("unexpected stream exhaust");
      }
      out.put(two_hex_to_char(hex[0], hex[1]));
      continue;
    }

    out.put(c);
    sb->sbumpc();
  }
}

Name parser::read_name(std::istream &in) {
  std::stringstream ss;
  read_name(in, ss);
  return ss.str();
}

bool parser::peek_null(std::istream &in) {
  std::istream::sentry se(in, true);
  std::streambuf *sb = in.rdbuf();

  int_type c = sb->sgetc();
  return c != eof && (c == 'n' || c == 'N');
}

void parser::read_null(std::istream &in) {
  std::istream::sentry se(in, true);
  std::streambuf *sb = in.rdbuf();

  char tmp[4];
  if (sb->sgetn(tmp, 4) != 4) {
    throw std::runtime_error("unexpected stream exhaust");
  }
  // TODO check ignorecase
}

bool parser::peek_boolean(std::istream &in) {
  std::istream::sentry se(in, true);
  std::streambuf *sb = in.rdbuf();

  int_type c = sb->sgetc();
  return c != eof && (c == 't' || c == 'T' || c == 'f' || c == 'F');
}

Boolean parser::read_boolean(std::istream &in) {
  std::istream::sentry se(in, true);
  std::streambuf *sb = in.rdbuf();

  int_type c = sb->sgetc();

  if (c == 't' || c == 'T') {
    char tmp[4];
    if (sb->sgetn(tmp, 4) != 4) {
      throw std::runtime_error("unexpected stream exhaust");
    }
    // TODO check ignore case

    return true;
  }

  if (c == 'f' || c == 'F') {
    char tmp[5];
    if (sb->sgetn(tmp, 5) != 5) {
      throw std::runtime_error("unexpected stream exhaust");
    }
    // TODO check ignore case

    return false;
  }

  throw std::runtime_error("unexpected starting character");
}

bool parser::peek_string(std::istream &in) {
  std::istream::sentry se(in, true);
  std::streambuf *sb = in.rdbuf();

  int_type c = sb->sgetc();
  if (c == eof) {
    return false;
  }
  if (c == '(') {
    return true;
  }
  if (c == '<') {
    sb->sbumpc();
    c = sb->sgetc();
    if (sb->sungetc() == eof) {
      throw std::runtime_error("unexpected stream exhaust");
    }
    return c != '<';
  }
  return false;
}

void parser::read_string(std::istream &in, std::ostream &out) {
  std::istream::sentry se(in, true);
  std::streambuf *sb = in.rdbuf();

  int_type c = sb->sbumpc();

  if (c == '(') {
    while (true) {
      c = sb->sbumpc();

      if (c == eof) {
        in.setstate(std::ios::eofbit);
        throw std::runtime_error("unexpected stream exhaust");
      }
      if (c == ')') {
        return;
      }

      out.put(c);
    }
  }

  if (c == '<') {
    while (true) {
      c = sb->sbumpc();

      if (c == eof) {
        in.setstate(std::ios::eofbit);
        throw std::runtime_error("unexpected stream exhaust");
      }
      if (c == '>') {
        return;
      }

      int_type c2 = sb->sbumpc();

      if (c2 == eof) {
        in.setstate(std::ios::eofbit);
        throw std::runtime_error("unexpected stream exhaust");
      }

      out.put(two_hex_to_char(c, c2));
    }
  }

  throw std::runtime_error("unexpected starting character");
}

String parser::read_string(std::istream &in) {
  std::stringstream ss;
  read_string(in, ss);
  return ss.str();
}

bool parser::peek_array(std::istream &in) {
  std::istream::sentry se(in, true);
  std::streambuf *sb = in.rdbuf();

  int_type c = sb->sgetc();
  return c != eof && c == '[';
}

Array parser::read_array(std::istream &in) {
  std::istream::sentry se(in, true);
  std::streambuf *sb = in.rdbuf();

  if (sb->sbumpc() != '[') {
    throw std::runtime_error("unexpected character");
  }

  Array result;

  while (true) {
    skip_whitespace(in);
    if (sb->sgetc() == ']') {
      sb->sbumpc();
      return result;
    }
    Object value = read_object(in);
    result.emplace_back(std::move(value));
  }
}

bool parser::peek_dictionary(std::istream &in) {
  std::istream::sentry se(in, true);
  std::streambuf *sb = in.rdbuf();

  int_type c = sb->sgetc();
  if (c == eof) {
    return false;
  }
  if (c == '<') {
    sb->sbumpc();
    c = sb->sgetc();
    if (sb->sungetc() == eof) {
      throw std::runtime_error("unexpected stream exhaust");
    }
    return c == '<';
  }
  return false;
}

Dictionary parser::read_dictionary(std::istream &in) {
  std::istream::sentry se(in, true);
  std::streambuf *sb = in.rdbuf();

  if (sb->sbumpc() != '<') {
    throw std::runtime_error("unexpected character");
  }
  if (sb->sbumpc() != '<') {
    throw std::runtime_error("unexpected character");
  }

  Dictionary result;

  while (true) {
    skip_whitespace(in);
    if (sb->sgetc() == '>') {
      sb->sbumpc();
      sb->sbumpc();
      return result;
    }
    Name name = read_name(in);
    skip_whitespace(in);
    Object value = read_object(in);
    skip_whitespace(in);

    // Handle indirect objects
    // TODO this seems hacky
    if (!peek_name(in)) {
      UnsignedInteger gen = read_unsigned_integer(in);
      skip_whitespace(in);

      if (sb->sbumpc() != 'R') {
        throw std::runtime_error("unexpected character");
      }

      value = ObjectReference{std::any_cast<Integer>(value), gen};
    }

    result.emplace(std::move(name), std::move(value));
  }
}

Object parser::read_object(std::istream &in) {
  std::istream::sentry se(in, true);
  std::streambuf *sb = in.rdbuf();

  int_type c = sb->sgetc();

  if (c == eof) {
    in.setstate(std::ios::eofbit);
    throw std::runtime_error("unexpected stream exhaust");
  }

  if (peek_null(in)) {
    return {};
  }
  if (peek_boolean(in)) {
    return read_boolean(in);
  }
  if (peek_number(in)) {
    return std::visit([](auto v) -> Object { return v; },
                      read_integer_or_real(in));
  }
  if (peek_string(in)) {
    return read_string(in);
  }
  if (peek_array(in)) {
    return read_array(in);
  }
  if (peek_dictionary(in)) {
    return read_dictionary(in);
  }

  throw std::runtime_error("unknown object");
}

ObjectReference parser::read_object_reference(std::istream &in) {
  std::istream::sentry se(in, true);
  std::streambuf *sb = in.rdbuf();

  UnsignedInteger id = read_unsigned_integer(in);
  skip_whitespace(in);
  UnsignedInteger gen = read_unsigned_integer(in);
  skip_whitespace(in);

  if (sb->sbumpc() != 'R') {
    throw std::runtime_error("unexpected character");
  }

  return {id, gen};
}

} // namespace odr::internal::pdf
