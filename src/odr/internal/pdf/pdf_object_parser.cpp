#include <odr/internal/pdf/pdf_object_parser.hpp>

#include <odr/internal/util/number_util.hpp>
#include <odr/internal/util/stream_util.hpp>
#include <odr/internal/util/string_util.hpp>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace odr::internal::pdf {

namespace {

template <typename T> T parse_integer(std::string_view token) {
  if constexpr (std::is_signed_v<T>) {
    if (token.starts_with('+')) {
      token.remove_prefix(1);
      if (token.starts_with('-')) {
        throw std::runtime_error("invalid PDF integer");
      }
    }
  }
  T value{};
  const auto [end, error] =
      std::from_chars(token.data(), token.data() + token.size(), value);
  if (error != std::errc{} || end != token.data() + token.size()) {
    throw std::runtime_error("invalid or out-of-range PDF integer");
  }
  return value;
}

} // namespace

ObjectParser::ObjectParser(std::istream &in) : m_in{&in}, m_sb{in.rdbuf()} {
  // One-time stream preparation (flush tied streams, state check) for the
  // raw-streambuf reads below; a sentry's effects live entirely in its
  // constructor, so it is not kept as state (it would also make the parser
  // immovable).
  const std::istream::sentry se(in, true);
}

ObjectParser::NestingGuard::NestingGuard(ObjectParser &parser)
    : m_parser{&parser} {
  if (++m_parser->m_depth > max_nesting_depth) {
    // the guard never finishes constructing, so nothing will undo this
    --m_parser->m_depth;
    throw std::runtime_error("object nesting too deep");
  }
}

ObjectParser::NestingGuard::~NestingGuard() { --m_parser->m_depth; }

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

std::uint8_t ObjectParser::hex_char_to_int(const char_type c) {
  const std::optional<std::uint8_t> value = util::string::hex_digit(c);
  if (!value) {
    throw std::runtime_error("invalid character in hex_char_to_int");
  }
  return *value;
}

ObjectParser::char_type ObjectParser::two_hex_to_char(const char_type first,
                                                      const char_type second) {
  return static_cast<char_type>(hex_char_to_int(first) * 16 +
                                hex_char_to_int(second));
}

bool ObjectParser::is_whitespace(const char c) {
  return c == '\0' || c == '\t' || c == '\n' || c == '\f' || c == '\r' ||
         c == ' ';
}

bool ObjectParser::is_delimiter(const char c) {
  return c == '(' || c == ')' || c == '<' || c == '>' || c == '[' || c == ']' ||
         c == '{' || c == '}' || c == '/' || c == '%';
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

void ObjectParser::skip_whitespace_and_comments() {
  while (true) {
    skip_whitespace();
    if (geti() != '%') {
      return;
    }
    while (true) {
      const int_type c = sb().sbumpc();
      if (c == eof) {
        in().setstate(std::ios::eofbit);
        return;
      }
      if (c == '\n' || c == '\r') {
        break;
      }
    }
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

/// The keyword is lowercase in ISO 32000-1 7.3.2, but the `peek_` above take
/// either case, so what follows has to as well - and it has to be read, or
/// `nXYZ` parses as null.
void ObjectParser::expect_keyword(const std::string &keyword) {
  const std::string observed = bumpnc(keyword.size());
  if (!std::ranges::equal(observed, keyword, [](char a, char b) {
        return std::tolower(static_cast<unsigned char>(a)) ==
               std::tolower(static_cast<unsigned char>(b));
      })) {
    throw std::runtime_error("unexpected keyword (expected: " + keyword +
                             ", observed: " + observed + ")");
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

std::string ObjectParser::read_keyword() {
  std::string result;
  while (true) {
    const int_type c = geti();
    if (c == eof || is_whitespace(static_cast<char_type>(c)) ||
        is_delimiter(static_cast<char_type>(c))) {
      return result;
    }
    result += bumpc();
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

UnsignedInteger ObjectParser::read_unsigned_integer() {
  return parse_integer<UnsignedInteger>(read_keyword());
}

Integer ObjectParser::read_integer() {
  return parse_integer<Integer>(read_keyword());
}

Real ObjectParser::read_number() {
  return std::visit([](auto value) -> Real { return value; },
                    read_integer_or_real());
}

std::variant<Integer, Real> ObjectParser::read_integer_or_real() {
  const std::string token = read_keyword();
  std::string_view magnitude(token);
  if (magnitude.starts_with('+') || magnitude.starts_with('-')) {
    magnitude.remove_prefix(1);
  }

  // ISO 32000-1 7.3.3 has neither an exponent nor a limit, but producers write
  // both; like pdf.js, `1e3` and an integer past the range read as reals.
  const std::size_t exponent = magnitude.find_first_of("eE");
  const std::string_view mantissa = magnitude.substr(0, exponent);
  if (mantissa.empty() || mantissa == "." ||
      mantissa.find_first_not_of("0123456789.") != std::string_view::npos ||
      std::ranges::count(mantissa, '.') > 1) {
    throw std::runtime_error("invalid PDF number");
  }
  if (exponent != std::string_view::npos) {
    std::string_view power = magnitude.substr(exponent + 1);
    if (power.starts_with('+') || power.starts_with('-')) {
      power.remove_prefix(1);
    }
    if (power.empty() ||
        power.find_first_not_of("0123456789") != std::string_view::npos) {
      throw std::runtime_error("invalid PDF number");
    }
  } else if (mantissa.find('.') == std::string_view::npos) {
    const std::string_view digits =
        token.starts_with('+') ? std::string_view(token).substr(1) : token;
    Integer value{};
    const auto [end, error] =
        std::from_chars(digits.data(), digits.data() + digits.size(), value);
    if (error == std::errc{} && end == digits.data() + digits.size()) {
      return value;
    }
  }

  const auto value = util::number::parse(token);
  if (!value || !std::isfinite(*value)) {
    throw std::runtime_error("out-of-range PDF number");
  }
  return *value;
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

void ObjectParser::read_null() { expect_keyword("null"); }

bool ObjectParser::peek_boolean() {
  const int_type c = geti();
  return c != eof && (c == 't' || c == 'T' || c == 'f' || c == 'F');
}

Boolean ObjectParser::read_boolean() {
  const int_type c = geti();

  if (c == 't' || c == 'T') {
    expect_keyword("true");
    return true;
  }

  if (c == 'f' || c == 'F') {
    expect_keyword("false");
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
    // 7.3.4.2: a balanced pair of parentheses inside a literal string needs no
    // escaping, so only the `)` closing the outermost pair ends the string.
    std::uint32_t depth = 1;

    while (true) {
      c = bumpc();

      if (c == '\\') {
        c = getc();
        if (c >= '0' && c <= '7') {
          // ISO 32000-1 7.3.4.2: one to three octal digits, modulo 256.
          std::uint32_t value = 0;
          for (std::uint32_t digits = 0; digits < 3; ++digits) {
            const int_type next = geti();
            if (next < '0' || next > '7') {
              break;
            }
            value = value * 8 + (bumpc() - '0');
          }
          string += static_cast<char>(value & 0xff);
        } else {
          bumpc();
          // Reverse-solidus escapes (ISO 32000-1 7.3.4.2, Table 3). A
          // backslash before an end-of-line marker is a line continuation;
          // anything else unlisted stands for itself.
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
      if (c == '\r') {
        if (geti() == '\n') {
          bumpc();
        }
        c = '\n';
      }
      if (c == '(') {
        ++depth;
      } else if (c == ')') {
        --depth;
        if (depth == 0) {
          return StandardString(std::move(string));
        }
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
  const NestingGuard guard(*this);
  Array::Holder result;

  if (bumpc() != '[') {
    throw std::runtime_error("unexpected character");
  }
  skip_whitespace_and_comments();

  while (true) {
    if (const char_type c = getc(); c == ']') {
      bumpc();
      return Array(std::move(result));
    }
    result.emplace_back(read_object());
    skip_whitespace_and_comments();

    // Array elements may be bare adjacent integers, so a reference can only be
    // recognised once the `R` token actually appears: it retroactively folds
    // the two preceding integers (`n g`) into an `n g R` reference.
    if (const char_type c = getc(); c == 'R' && result.size() >= 2) {
      bumpc();
      skip_whitespace_and_comments();

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
  const NestingGuard guard(*this);
  Dictionary::Holder result;

  if (bumpc() != '<') {
    throw std::runtime_error("unexpected character");
  }
  if (bumpc() != '<') {
    throw std::runtime_error("unexpected character");
  }
  skip_whitespace_and_comments();

  while (true) {
    if (const char_type c = getc(); c == '>') {
      expect_characters(">>");
      return Dictionary(std::move(result));
    }

    Name name = read_name();
    skip_whitespace_and_comments();
    Object value = read_object();
    skip_whitespace_and_comments();
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
  // Called only where a value cannot legitimately be followed by another
  // number — the next token is a key, `>>` or `endobj` — so a digit here can
  // only be the `g` of an `n g R` whose object number is `value`.
  if (!value.is_integer() || !peek_unsigned_integer()) {
    return;
  }
  const auto id = static_cast<UnsignedInteger>(value.as_integer());
  const UnsignedInteger gen = read_unsigned_integer();
  skip_whitespace_and_comments();
  if (bumpc() != 'R') {
    throw std::runtime_error("expected 'R' to complete indirect reference");
  }
  skip_whitespace_and_comments();
  value = Object(ObjectReference{id, gen});
}

ObjectReference ObjectParser::read_object_reference() {
  UnsignedInteger id = read_unsigned_integer();
  skip_whitespace_and_comments();
  UnsignedInteger gen = read_unsigned_integer();
  skip_whitespace_and_comments();

  if (bumpc() != 'R') {
    throw std::runtime_error("unexpected character");
  }

  return {id, gen};
}

} // namespace odr::internal::pdf
