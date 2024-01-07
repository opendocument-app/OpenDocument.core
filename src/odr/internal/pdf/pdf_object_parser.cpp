#include <odr/internal/pdf/pdf_object_parser.hpp>

#include <odr/internal/util/stream_util.hpp>

#include <cmath>
#include <sstream>
#include <stdexcept>

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

ObjectParser::ObjectParser(std::istream &in)
    : m_in{&in}, m_se(in, true), m_sb{in.rdbuf()} {}

std::istream &ObjectParser::in() const { return *m_in; }

std::streambuf &ObjectParser::sb() const { return *m_sb; }

void ObjectParser::skip_whitespace() const {
  while (true) {
    int_type c = sb().sgetc();
    switch (c) {
    case '\0':
    case '\t':
    case '\n':
    case '\f':
    case '\r':
    case ' ':
      sb().sbumpc();
      break;
    case eof:
      in().setstate(std::ios::eofbit);
    default:
      return;
    }
  }
}

void ObjectParser::skip_line() const { read_line(); }

std::string ObjectParser::read_line(bool inclusive) const {
  return util::stream::read_line(in(), inclusive);
}

bool ObjectParser::peek_number() const {
  int_type c = sb().sgetc();
  return c != eof && (c == '+' || c == '-' || c == '.' || std::isdigit(c));
}

UnsignedInteger ObjectParser::read_unsigned_integer() const {
  UnsignedInteger result = 0;

  while (true) {
    int_type c = sb().sgetc();
    if (c == eof) {
      in().setstate(std::ios::eofbit);
      return result;
    }
    if (!std::isdigit(c)) {
      return result;
    }
    result = result * 10 + (c - '0');
    sb().sbumpc();
  }
}

Integer ObjectParser::read_integer() const {
  std::int8_t sign = 1;

  if (sb().sgetc() == '+') {
    sign = +1;
    sb().sbumpc();
  }
  if (sb().sgetc() == '-') {
    sign = -1;
    sb().sbumpc();
  }

  return sign * read_unsigned_integer();
}

Real ObjectParser::read_number() const {
  return std::visit([](auto v) -> Real { return v; }, read_integer_or_real());
}

std::variant<Integer, Real> ObjectParser::read_integer_or_real() const {
  Integer i = 0;

  int_type c = sb().sgetc();
  if (c != '.') {
    i = read_integer();
    c = sb().sgetc();
    if (c != '.') {
      return i;
    }
  }
  sb().sbumpc();

  std::streamsize begin = in().gcount();
  UnsignedInteger i2 = read_unsigned_integer();
  std::streamsize end = in().gcount();

  return i + i2 * std::pow(10.0, begin - end);
}

bool ObjectParser::peek_name() const {
  int_type c = sb().sgetc();
  return c != eof && c == '/';
}

void ObjectParser::read_name(std::ostream &out) const {
  if (int_type c = sb().sbumpc(); c != '/') {
    throw std::runtime_error("not a name");
  }

  while (true) {
    int_type c = sb().sgetc();

    if (c == eof) {
      in().setstate(std::ios::eofbit);
      return;
    }
    if (c < 0x21 || c > 0x7e || c == '/' || c == '%' || c == '(' || c == ')' ||
        c == '<' || c == '>' || c == '[' || c == ']' || c == '{' || c == '}') {
      return;
    }

    if (c == '#') {
      sb().sbumpc();
      char hex[2];
      if (sb().sgetn(hex, 2) != 2) {
        throw std::runtime_error("unexpected stream exhaust");
      }
      out.put(two_hex_to_char(hex[0], hex[1]));
      continue;
    }

    out.put(c);
    sb().sbumpc();
  }
}

Name ObjectParser::read_name() const {
  std::stringstream ss;
  read_name(ss);
  return ss.str();
}

bool ObjectParser::peek_null() const {
  int_type c = sb().sgetc();
  return c != eof && (c == 'n' || c == 'N');
}

void ObjectParser::read_null() const {
  char tmp[4];
  if (sb().sgetn(tmp, 4) != 4) {
    throw std::runtime_error("unexpected stream exhaust");
  }
  // TODO check ignore case
}

bool ObjectParser::peek_boolean() const {
  int_type c = sb().sgetc();
  return c != eof && (c == 't' || c == 'T' || c == 'f' || c == 'F');
}

Boolean ObjectParser::read_boolean() const {
  int_type c = sb().sgetc();

  if (c == 't' || c == 'T') {
    char tmp[4];
    if (sb().sgetn(tmp, 4) != 4) {
      throw std::runtime_error("unexpected stream exhaust");
    }
    // TODO check ignore case

    return true;
  }

  if (c == 'f' || c == 'F') {
    char tmp[5];
    if (sb().sgetn(tmp, 5) != 5) {
      throw std::runtime_error("unexpected stream exhaust");
    }
    // TODO check ignore case

    return false;
  }

  throw std::runtime_error("unexpected starting character");
}

bool ObjectParser::peek_string() const {
  int_type c = sb().sgetc();
  if (c == eof) {
    return false;
  }
  if (c == '(') {
    return true;
  }
  if (c == '<') {
    sb().sbumpc();
    c = sb().sgetc();
    if (sb().sungetc() == eof) {
      throw std::runtime_error("unexpected stream exhaust");
    }
    return c != '<';
  }
  return false;
}

std::variant<StandardString, HexString> ObjectParser::read_string() const {
  std::string string;

  int_type c = sb().sbumpc();

  if (c == '(') {
    while (true) {
      c = sb().sbumpc();

      if (c == eof) {
        in().setstate(std::ios::eofbit);
        throw std::runtime_error("unexpected stream exhaust");
      }
      if (c == ')') {
        return StandardString(std::move(string));
      }

      // TODO handle '/' correctly

      string += (char_type)c;
    }
  }

  if (c == '<') {
    while (true) {
      c = sb().sbumpc();

      if (c == eof) {
        in().setstate(std::ios::eofbit);
        throw std::runtime_error("unexpected stream exhaust");
      }
      if (c == '>') {
        return HexString(std::move(string));
      }

      int_type c2 = sb().sbumpc();

      if (c2 == eof) {
        in().setstate(std::ios::eofbit);
        throw std::runtime_error("unexpected stream exhaust");
      }

      string += two_hex_to_char(c, c2);
    }
  }

  throw std::runtime_error("unexpected starting character");
}

bool ObjectParser::peek_array() const {
  int_type c = sb().sgetc();
  return c != eof && c == '[';
}

Array ObjectParser::read_array() const {
  Array::Holder result;

  if (sb().sbumpc() != '[') {
    throw std::runtime_error("unexpected character");
  }
  skip_whitespace();

  while (true) {
    if (int_type c = sb().sgetc(); c == ']') {
      sb().sbumpc();
      return Array(std::move(result));
    }
    Object value = read_object();
    skip_whitespace();

    result.emplace_back(std::move(value));

    if (int_type c = sb().sgetc(); c == 'R') {
      sb().sbumpc();
      skip_whitespace();

      UnsignedInteger gen = result.back().as_integer();
      result.pop_back();
      UnsignedInteger id = result.back().as_integer();
      result.pop_back();
      result.emplace_back(ObjectReference{id, gen});
    }
  }
}

bool ObjectParser::peek_dictionary() const {
  int_type c = sb().sgetc();
  if (c == eof) {
    return false;
  }
  if (c == '<') {
    sb().sbumpc();
    c = sb().sgetc();
    if (sb().sungetc() == eof) {
      throw std::runtime_error("unexpected stream exhaust");
    }
    return c == '<';
  }
  return false;
}

Dictionary ObjectParser::read_dictionary() const {
  Dictionary::Holder result;

  if (sb().sbumpc() != '<') {
    throw std::runtime_error("unexpected character");
  }
  if (sb().sbumpc() != '<') {
    throw std::runtime_error("unexpected character");
  }
  skip_whitespace();

  while (true) {
    if (int_type c = sb().sgetc(); c == '>') {
      sb().sbumpc();
      sb().sbumpc();
      return Dictionary(std::move(result));
    }

    Name name = read_name();
    skip_whitespace();
    Object value = read_object();
    skip_whitespace();

    // Handle indirect objects
    // TODO this seems hacky
    if (int_type c = sb().sgetc(); c != '>' && !peek_name()) {
      UnsignedInteger gen = read_unsigned_integer();
      skip_whitespace();
      if (int_type c2 = sb().sbumpc(); c2 != 'R') {
        throw std::runtime_error("unexpected character");
      }
      skip_whitespace();

      UnsignedInteger id = value.as_integer();
      value = ObjectReference{id, gen};
    }

    result.emplace(std::move(name.string), std::move(value));
  }
}

Object ObjectParser::read_object() const {
  int_type c = sb().sgetc();

  if (c == eof) {
    in().setstate(std::ios::eofbit);
    throw std::runtime_error("unexpected stream exhaust");
  }

  if (peek_null()) {
    read_null();
    return {};
  }
  if (peek_boolean()) {
    return read_boolean();
  }
  if (peek_number()) {
    return std::visit([](auto v) -> Object { return v; },
                      read_integer_or_real());
  }
  if (peek_name()) {
    return read_name();
  }
  if (peek_string()) {
    return std::visit([](auto v) -> Object { return v; }, read_string());
  }
  if (peek_array()) {
    return read_array();
  }
  if (peek_dictionary()) {
    return read_dictionary();
  }

  throw std::runtime_error("unknown object");
}

ObjectReference ObjectParser::read_object_reference() const {
  UnsignedInteger id = read_unsigned_integer();
  skip_whitespace();
  UnsignedInteger gen = read_unsigned_integer();
  skip_whitespace();

  if (sb().sbumpc() != 'R') {
    throw std::runtime_error("unexpected character");
  }

  return {id, gen};
}

} // namespace odr::internal::pdf
