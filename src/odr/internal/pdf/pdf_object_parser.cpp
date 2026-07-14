#include <odr/internal/pdf/pdf_object_parser.hpp>

#include <odr/internal/util/stream_util.hpp>

#include <cmath>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace odr::internal::pdf {

ObjectParser::ObjectParser(std::istream &in) : m_in{&in}, m_sb{in.rdbuf()} {
  // One-time stream preparation (flush tied streams, state check) for the
  // raw-streambuf reads below; a sentry's effects live entirely in its
  // constructor, so it is not kept as state (it would also make the parser
  // immovable).
  const std::istream::sentry se(in, true);
}

std::istream &ObjectParser::in() { return *m_in; }

std::streambuf &ObjectParser::sb() { return *m_sb; }

ObjectParser::int_type ObjectParser::geti() {
  const int_type c = sb().sgetc();
  if (c == eof) {
    in().setstate(std::ios::eofbit);
  }
  return c;
}

ObjectParser::char_type ObjectParser::getc() {
  const int_type c = sb().sgetc();
  if (c == eof) {
    in().setstate(std::ios::eofbit);
    throw std::runtime_error("unexpected stream exhaust");
  }
  return static_cast<char_type>(c);
}

ObjectParser::char_type ObjectParser::bumpc() {
  const int_type c = sb().sbumpc();
  if (c == eof) {
    in().setstate(std::ios::eofbit);
    throw std::runtime_error("unexpected stream exhaust");
  }
  return static_cast<char_type>(c);
}

std::string ObjectParser::bumpnc(const std::size_t n) {
  std::string result(n, '\0');
  if (const auto m = static_cast<std::streamsize>(n);
      sb().sgetn(result.data(), m) != m) {
    throw std::runtime_error("unexpected stream exhaust");
  }
  return result;
}

void ObjectParser::ungetc() {
  if (sb().sungetc() == eof) {
    throw std::runtime_error("unexpected stream exhaust");
  }
}

std::uint8_t ObjectParser::octet_char_to_int(const char_type c) {
  if (c >= '0' && c <= '7') {
    return c - '0';
  }
  throw std::runtime_error("invalid character in octet_char_to_int");
}

std::uint8_t ObjectParser::hex_char_to_int(const char_type c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  throw std::runtime_error("invalid character in hex_char_to_int");
}

ObjectParser::char_type ObjectParser::two_hex_to_char(const char_type first,
                                                      const char_type second) {
  return static_cast<char_type>(hex_char_to_int(first) * 16 +
                                hex_char_to_int(second));
}

ObjectParser::char_type
ObjectParser::three_octet_to_char(const char_type first, const char_type second,
                                  const char_type third) {
  return static_cast<char_type>(octet_char_to_int(first) * 64 +
                                octet_char_to_int(second) * 8 +
                                octet_char_to_int(third));
}

bool ObjectParser::is_whitespace(const char c) {
  return c == '\0' || c == '\t' || c == '\n' || c == '\f' || c == '\r' ||
         c == ' ';
}

bool ObjectParser::peek_whitespace() {
  const int_type c = geti();
  return c != eof && is_whitespace(static_cast<char_type>(c));
}

void ObjectParser::skip_whitespace() {
  while (true) {
    const int_type c = geti();
    if (c == eof) {
      return;
    }
    if (!is_whitespace(static_cast<char_type>(c))) {
      return;
    }
    bumpc();
  }
}

void ObjectParser::skip_line() { read_line(); }

std::string ObjectParser::read_line(const bool inclusive) {
  return util::stream::read_line(in(), inclusive);
}

bool ObjectParser::skip_past(const std::string_view marker) {
  if (marker.empty()) {
    return true;
  }

  // KMP failure function over the (typically tiny) marker, so the streaming
  // scan stays correct even when the marker has internal repetition (e.g. the
  // two `e`s in `endstream`).
  std::vector<std::size_t> fail(marker.size(), 0);
  for (std::size_t i = 1, k = 0; i < marker.size(); ++i) {
    while (k > 0 && marker[i] != marker[k]) {
      k = fail[k - 1];
    }
    if (marker[i] == marker[k]) {
      ++k;
    }
    fail[i] = k;
  }

  std::size_t matched = 0;
  while (true) {
    const int_type c = sb().sbumpc();
    if (c == eof) {
      in().setstate(std::ios::eofbit);
      return false;
    }
    const auto ch = static_cast<char_type>(c);
    while (matched > 0 && ch != marker[matched]) {
      matched = fail[matched - 1];
    }
    if (ch == marker[matched]) {
      ++matched;
      if (matched == marker.size()) {
        return true;
      }
    }
  }
}

void ObjectParser::expect_characters(const std::string &string) {
  const std::string observed = bumpnc(string.size());
  if (observed != string) {
    throw std::runtime_error("unexpected characters"
                             " (expected: " +
                             string + ", observed: " + observed + ")");
  }
}

bool ObjectParser::peek_number() {
  const int_type c = geti();
  return c != eof && (c == '+' || c == '-' || c == '.' || std::isdigit(c));
}

bool ObjectParser::peek_unsigned_integer() {
  const int_type c = geti();
  return c != eof && std::isdigit(c);
}

std::pair<UnsignedInteger, std::uint32_t>
ObjectParser::read_unsigned_integer_and_count() {
  UnsignedInteger result = 0;
  std::uint32_t count = 0;

  while (true) {
    const int_type c = geti();
    if (c == eof) {
      break;
    }
    if (!std::isdigit(c)) {
      break;
    }
    result = result * 10 + (c - '0');
    ++count;
    bumpc();
  }

  if (count == 0) {
    throw std::runtime_error("expected unsigned integer, but got none");
  }

  return {result, count};
}

UnsignedInteger ObjectParser::read_unsigned_integer() {
  return read_unsigned_integer_and_count().first;
}

Integer ObjectParser::read_integer() {
  Integer sign = 1;

  const char_type c = getc();
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

Real ObjectParser::read_number() {
  return std::visit([](auto v) -> Real { return v; }, read_integer_or_real());
}

std::variant<Integer, Real> ObjectParser::read_integer_or_real() {
  Integer sign = 1;
  if (geti() == '-') {
    sign = -1;
    bumpc();
  } else if (geti() == '+') {
    bumpc();
  }

  UnsignedInteger i = 0;

  if (geti() != '.') {
    i = read_unsigned_integer();
  }
  if (geti() != '.') {
    return static_cast<Integer>(sign * i);
  }
  bumpc();

  Real r = static_cast<Real>(i);

  if (peek_unsigned_integer()) {
    const auto [fraction, decimals] = read_unsigned_integer_and_count();
    // `decimals` is unsigned; negate as floating point to avoid wrap-around.
    r += static_cast<Real>(fraction) *
         std::pow(10.0, -static_cast<Real>(decimals));
  }

  return static_cast<Real>(sign) * r;
}

bool ObjectParser::peek_name() {
  const int_type c = geti();
  return c == '/';
}

void ObjectParser::read_name(std::ostream &out) {
  if (const char_type c = bumpc(); c != '/') {
    throw std::runtime_error("not a name");
  }

  while (true) {
    const int_type i = geti();

    if (i == eof) {
      return;
    }
    const auto c = static_cast<char_type>(i);
    if (c < 0x21 || c > 0x7e || c == '/' || c == '%' || c == '(' || c == ')' ||
        c == '<' || c == '>' || c == '[' || c == ']' || c == '{' || c == '}') {
      return;
    }

    if (c == '#') {
      bumpc();
      const std::array hex = bumpnc<2>();
      out.put(two_hex_to_char(hex[0], hex[1]));
      continue;
    }

    out.put(c);
    bumpc();
  }
}

Name ObjectParser::read_name() {
  std::stringstream ss;
  read_name(ss);
  return Name(ss.str());
}

bool ObjectParser::peek_null() {
  const int_type c = geti();
  return c != eof && (c == 'n' || c == 'N');
}

void ObjectParser::read_null() {
  std::ignore = bumpnc<4>();
  // TODO check ignore case
}

bool ObjectParser::peek_boolean() {
  const int_type c = geti();
  return c != eof && (c == 't' || c == 'T' || c == 'f' || c == 'F');
}

Boolean ObjectParser::read_boolean() {
  const int_type c = geti();

  if (c == 't' || c == 'T') {
    std::ignore = bumpnc<4>();
    // TODO check ignore case

    return true;
  }

  if (c == 'f' || c == 'F') {
    std::ignore = bumpnc<5>();
    // TODO check ignore case

    return false;
  }

  throw std::runtime_error("unexpected starting character");
}

bool ObjectParser::peek_string() {
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

std::variant<StandardString, HexString> ObjectParser::read_string() {
  std::string string;

  char_type c = bumpc();

  if (c == '(') {
    while (true) {
      c = bumpc();

      if (c == '\\') {
        c = getc();
        if (std::isdigit(c)) {
          const std::array octet = bumpnc<3>();
          string += three_octet_to_char(octet[0], octet[1], octet[2]);
        } else {
          bumpc();
          // Reverse-solidus escapes (ISO 32000-1 7.3.4.2, Table 3): the control
          // escapes translate to their byte value, a backslash before an
          // end-of-line marker is a line continuation (both elided), and any
          // other escaped character (including `(`, `)`, `\`) stands for
          // itself.
          switch (c) {
          case 'n':
            string += '\n';
            break;
          case 'r':
            string += '\r';
            break;
          case 't':
            string += '\t';
            break;
          case 'b':
            string += '\b';
            break;
          case 'f':
            string += '\f';
            break;
          case '\n':
            break;
          case '\r':
            if (geti() == '\n') {
              bumpc();
            }
            break;
          default:
            string += c;
            break;
          }
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
    // 7.3.4.3: whitespace between digits is ignored, and an odd final digit is
    // assumed to be followed by a 0.
    std::optional<int_type> high;
    while (true) {
      c = bumpc();

      if (c == '>') {
        if (high.has_value()) {
          string.push_back(static_cast<char_type>(*high << 4));
        }
        return HexString(std::move(string));
      }
      if (is_whitespace(c)) {
        continue;
      }

      const int_type nibble = hex_char_to_int(c);
      if (high.has_value()) {
        string.push_back(static_cast<char_type>((*high << 4) | nibble));
        high.reset();
      } else {
        high = nibble;
      }
    }
  }

  throw std::runtime_error("unexpected starting character");
}

bool ObjectParser::peek_array() {
  const int_type c = geti();
  return c == '[';
}

Array ObjectParser::read_array() {
  Array::Holder result;

  if (bumpc() != '[') {
    throw std::runtime_error("unexpected character");
  }
  skip_whitespace();

  while (true) {
    if (const char_type c = getc(); c == ']') {
      bumpc();
      return Array(std::move(result));
    }
    result.emplace_back(read_object());
    skip_whitespace();

    // Array elements may be bare adjacent integers, so a reference can only be
    // recognised once the `R` token actually appears: it retroactively folds
    // the two preceding integers (`n g`) into an `n g R` reference.
    if (const char_type c = getc(); c == 'R' && result.size() >= 2) {
      bumpc();
      skip_whitespace();

      const UnsignedInteger gen = result.back().as_integer();
      result.pop_back();
      const UnsignedInteger id = result.back().as_integer();
      result.pop_back();
      result.emplace_back(ObjectReference(id, gen));
    }
  }
}

bool ObjectParser::peek_dictionary() {
  const int_type i = geti();
  if (i == eof) {
    return false;
  }
  if (auto c = static_cast<char_type>(i); c == '<') {
    bumpc();
    c = getc();
    ungetc();
    return c == '<';
  }
  return false;
}

Dictionary ObjectParser::read_dictionary() {
  Dictionary::Holder result;

  if (bumpc() != '<') {
    throw std::runtime_error("unexpected character");
  }
  if (bumpc() != '<') {
    throw std::runtime_error("unexpected character");
  }
  skip_whitespace();

  while (true) {
    if (const char_type c = getc(); c == '>') {
      bumpc();
      bumpc();
      return Dictionary(std::move(result));
    }

    Name name = read_name();
    skip_whitespace();
    Object value = read_object();
    skip_whitespace();
    promote_indirect_reference(value);

    result.emplace(std::move(name.string), std::move(value));
  }
}

Object ObjectParser::read_object() {
  getc();

  if (peek_null()) {
    read_null();
    return {};
  }
  if (peek_boolean()) {
    return Object(read_boolean());
  }
  if (peek_number()) {
    return std::visit([](auto v) -> Object { return Object(v); },
                      read_integer_or_real());
  }
  if (peek_name()) {
    return Object(read_name());
  }
  if (peek_string()) {
    return std::visit([](auto v) -> Object { return Object(std::move(v)); },
                      read_string());
  }
  if (peek_array()) {
    return Object(read_array());
  }
  if (peek_dictionary()) {
    return Object(read_dictionary());
  }

  throw std::runtime_error("unknown object");
}

void ObjectParser::promote_indirect_reference(Object &value) {
  // The cursor sits just past `value` (trailing whitespace already skipped).
  // This is called only where a value cannot legitimately be followed by
  // another number — a dictionary value or an indirect-object body, which are
  // followed by the next key, `>>`, or `endobj`. A digit there can only be the
  // generation of an `n g R` indirect reference whose object number is `value`.
  if (!value.is_integer() || !peek_unsigned_integer()) {
    return;
  }
  const auto id = static_cast<UnsignedInteger>(value.as_integer());
  const UnsignedInteger gen = read_unsigned_integer();
  skip_whitespace();
  if (bumpc() != 'R') {
    throw std::runtime_error("expected 'R' to complete indirect reference");
  }
  skip_whitespace();
  value = Object(ObjectReference{id, gen});
}

ObjectReference ObjectParser::read_object_reference() {
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
