#include <odr/internal/pdf/pdf_object_parser.hpp>

#include <odr/internal/util/stream_util.hpp>

#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace odr::internal::pdf {

ObjectParser::ObjectParser(std::istream &in)
    : m_in{&in}, m_se(in, true), m_sb{in.rdbuf()} {}

std::istream &ObjectParser::in() const { return *m_in; }

std::streambuf &ObjectParser::sb() const { return *m_sb; }

ObjectParser::int_type ObjectParser::geti() const {
  int_type c = sb().sgetc();
  if (c == eof) {
    in().setstate(std::ios::eofbit);
  }
  return c;
}

ObjectParser::char_type ObjectParser::getc() const {
  int_type c = sb().sgetc();
  if (c == eof) {
    in().setstate(std::ios::eofbit);
    throw std::runtime_error("unexpected stream exhaust");
  }
  return static_cast<char_type>(c);
}

ObjectParser::char_type ObjectParser::bumpc() const {
  int_type c = sb().sbumpc();
  if (c == eof) {
    in().setstate(std::ios::eofbit);
    throw std::runtime_error("unexpected stream exhaust");
  }
  return static_cast<char_type>(c);
}

std::string ObjectParser::bumpnc(std::size_t n) const {
  std::string result(n, '\0');
  auto m = static_cast<std::streamsize>(n);
  if (sb().sgetn(result.data(), m) != m) {
    throw std::runtime_error("unexpected stream exhaust");
  }
  return result;
}

void ObjectParser::ungetc() const {
  if (sb().sungetc() == eof) {
    throw std::runtime_error("unexpected stream exhaust");
  }
}

ObjectParser::int_type ObjectParser::octet_char_to_int(char_type c) {
  return c - '0';
}

ObjectParser::int_type ObjectParser::hex_char_to_int(char_type c) {
  if (c >= 'a') {
    return 10 + (c - 'a');
  }
  if (c >= 'A') {
    return 10 + (c - 'A');
  }
  return c - '0';
}

ObjectParser::char_type ObjectParser::two_hex_to_char(char_type first,
                                                      char_type second) {
  return static_cast<char_type>(hex_char_to_int(first) * 16 +
                                hex_char_to_int(second));
}

ObjectParser::char_type ObjectParser::three_octet_to_char(char_type first,
                                                          char_type second,
                                                          char_type third) {
  return static_cast<char_type>(octet_char_to_int(first) * 64 +
                                octet_char_to_int(second) * 8 +
                                octet_char_to_int(third));
}

bool ObjectParser::is_whitespace(char c) {
  return c == '\0' || c == '\t' || c == '\n' || c == '\f' || c == '\r' ||
         c == ' ';
}

bool ObjectParser::peek_whitespace() const {
  int_type c = geti();
  return c != eof && is_whitespace(static_cast<char_type>(c));
}

void ObjectParser::skip_whitespace() const {
  while (true) {
    int_type c = geti();
    if (c == eof) {
      return;
    }
    if (!is_whitespace(static_cast<char_type>(c))) {
      return;
    }
    bumpc();
  }
}

void ObjectParser::skip_line() const { read_line(); }

std::string ObjectParser::read_line(bool inclusive) const {
  return util::stream::read_line(in(), inclusive);
}

void ObjectParser::expect_characters(const std::string &string) const {
  auto observed = bumpnc(string.size());
  if (observed != string) {
    throw std::runtime_error("unexpected characters"
                             " (expected: " +
                             string + ", observed: " + observed + ")");
  }
}

bool ObjectParser::peek_number() const {
  int_type c = geti();
  return c != eof && (c == '+' || c == '-' || c == '.' || std::isdigit(c));
}

UnsignedInteger ObjectParser::read_unsigned_integer() const {
  UnsignedInteger result = 0;

  while (true) {
    int_type c = geti();
    if (c == eof) {
      return result;
    }
    if (!std::isdigit(c)) {
      return result;
    }
    result = result * 10 + (c - '0');
    bumpc();
  }
}

Integer ObjectParser::read_integer() const {
  Integer sign = 1;

  char_type c = getc();
  if (c == '+') {
    sign = +1;
    bumpc();
  }
  if (c == '-') {
    sign = -1;
    bumpc();
  }

  return sign * static_cast<Integer>(read_unsigned_integer());
}

Real ObjectParser::read_number() const {
  return std::visit([](auto v) -> Real { return v; }, read_integer_or_real());
}

std::variant<Integer, Real> ObjectParser::read_integer_or_real() const {
  Integer i = 0;

  char_type c = getc();
  if (c != '.') {
    i = read_integer();
    c = getc();
    if (c != '.') {
      return i;
    }
  }
  bumpc();

  pos_type begin = in().tellg();
  UnsignedInteger i2 = read_unsigned_integer();
  pos_type end = in().tellg();

  return static_cast<Real>(i) +
         static_cast<Real>(i2) * std::pow(10.0, begin - end);
}

bool ObjectParser::peek_name() const {
  int_type c = geti();
  return c == '/';
}

void ObjectParser::read_name(std::ostream &out) const {
  if (char_type c = bumpc(); c != '/') {
    throw std::runtime_error("not a name");
  }

  while (true) {
    int_type i = geti();

    if (i == eof) {
      return;
    }
    auto c = static_cast<char_type>(i);
    if (c < 0x21 || c > 0x7e || c == '/' || c == '%' || c == '(' || c == ')' ||
        c == '<' || c == '>' || c == '[' || c == ']' || c == '{' || c == '}') {
      return;
    }

    if (c == '#') {
      bumpc();
      auto hex = bumpnc<2>();
      out.put(two_hex_to_char(hex[0], hex[1]));
      continue;
    }

    out.put(c);
    bumpc();
  }
}

Name ObjectParser::read_name() const {
  std::stringstream ss;
  read_name(ss);
  return ss.str();
}

bool ObjectParser::peek_null() const {
  int_type c = geti();
  return c != eof && (c == 'n' || c == 'N');
}

void ObjectParser::read_null() const {
  auto tmp = bumpnc<4>();
  // TODO check ignore case
  (void)tmp;
}

bool ObjectParser::peek_boolean() const {
  int_type c = geti();
  return c != eof && (c == 't' || c == 'T' || c == 'f' || c == 'F');
}

Boolean ObjectParser::read_boolean() const {
  int_type c = geti();

  if (c == 't' || c == 'T') {
    auto tmp = bumpnc<4>();
    // TODO check ignore case
    (void)tmp;

    return true;
  }

  if (c == 'f' || c == 'F') {
    auto tmp = bumpnc<5>();
    // TODO check ignore case
    (void)tmp;

    return false;
  }

  throw std::runtime_error("unexpected starting character");
}

bool ObjectParser::peek_string() const {
  int_type c = geti();
  if (c == eof) {
    return false;
  }
  if (c == '(') {
    return true;
  }
  if (c == '<') {
    bumpc();
    c = geti();
    ungetc();
    return c != '<';
  }
  return false;
}

std::variant<StandardString, HexString> ObjectParser::read_string() const {
  std::string string;

  char_type c = bumpc();

  if (c == '(') {
    while (true) {
      c = bumpc();

      if (c == '\\') {
        c = getc();
        if (std::isdigit(c)) {
          auto octet = bumpnc<3>();
          string += three_octet_to_char(octet[0], octet[1], octet[2]);
        } else {
          bumpc();
          string += c;
        }
        continue;
      }
      if (c == ')') {
        return StandardString(std::move(string));
      }

      string += c;
    }
  }

  if (c == '<') {
    while (true) {
      c = bumpc();

      if (c == '>') {
        return HexString(std::move(string));
      }

      char_type c2 = bumpc();
      string += two_hex_to_char(c, c2);
    }
  }

  throw std::runtime_error("unexpected starting character");
}

bool ObjectParser::peek_array() const {
  int_type c = geti();
  return c == '[';
}

Array ObjectParser::read_array() const {
  Array::Holder result;

  if (bumpc() != '[') {
    throw std::runtime_error("unexpected character");
  }
  skip_whitespace();

  while (true) {
    if (char_type c = getc(); c == ']') {
      bumpc();
      return Array(std::move(result));
    }
    Object value = read_object();
    skip_whitespace();

    result.emplace_back(std::move(value));

    if (char_type c = getc(); c == 'R') {
      bumpc();
      skip_whitespace();

      UnsignedInteger gen = result.back().as_integer();
      result.pop_back();
      UnsignedInteger id = result.back().as_integer();
      result.pop_back();
      result.emplace_back(ObjectReference(id, gen));
    }
  }
}

bool ObjectParser::peek_dictionary() const {
  int_type i = geti();
  if (i == eof) {
    return false;
  }
  auto c = static_cast<char_type>(i);
  if (c == '<') {
    bumpc();
    c = getc();
    ungetc();
    return c == '<';
  }
  return false;
}

Dictionary ObjectParser::read_dictionary() const {
  Dictionary::Holder result;

  if (bumpc() != '<') {
    throw std::runtime_error("unexpected character");
  }
  if (bumpc() != '<') {
    throw std::runtime_error("unexpected character");
  }
  skip_whitespace();

  while (true) {
    if (char_type c = getc(); c == '>') {
      bumpc();
      bumpc();
      return Dictionary(std::move(result));
    }

    Name name = read_name();
    skip_whitespace();
    Object value = read_object();
    skip_whitespace();

    // Handle indirect objects
    // TODO this seems hacky
    if (char_type c = getc(); c != '>' && !peek_name()) {
      UnsignedInteger gen = read_unsigned_integer();
      skip_whitespace();
      if (char_type c2 = bumpc(); c2 != 'R') {
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
  getc();

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

  if (bumpc() != 'R') {
    throw std::runtime_error("unexpected character");
  }

  return {id, gen};
}

} // namespace odr::internal::pdf
