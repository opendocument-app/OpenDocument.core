#include <odr/internal/pdf/pdf_parser.hpp>

#include <odr/internal/util/stream_util.hpp>
#include <odr/internal/util/string_util.hpp>

#include <cmath>
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

PdfObjectParser::PdfObjectParser(std::istream &in)
    : m_in{&in}, m_se(in, true), m_sb{in.rdbuf()} {}

std::istream &PdfObjectParser::in() const { return *m_in; }

std::streambuf &PdfObjectParser::sb() const { return *m_sb; }

void PdfObjectParser::skip_whitespace() const {
  while (true) {
    int_type c = sb().sgetc();
    switch (c) {
    case ' ':
    case '\n':
      sb().sbumpc();
      break;
    case eof:
      in().setstate(std::ios::eofbit);
    default:
      return;
    }
  }
}

void PdfObjectParser::skip_line() const { read_line(); }

std::string PdfObjectParser::read_line() const {
  return util::stream::read_until(in(), '\n', false);
}

bool PdfObjectParser::peek_number() const {
  int_type c = sb().sgetc();
  return c != eof && (c == '+' || c == '-' || c == '.' || std::isdigit(c));
}

UnsignedInteger PdfObjectParser::read_unsigned_integer() const {
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

Integer PdfObjectParser::read_integer() const {
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

Real PdfObjectParser::read_number() const {
  return std::visit([](auto v) -> Real { return v; }, read_integer_or_real());
}

IntegerOrReal PdfObjectParser::read_integer_or_real() const {
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

bool PdfObjectParser::peek_name() const {
  int_type c = sb().sgetc();
  return c != eof && c == '/';
}

void PdfObjectParser::read_name(std::ostream &out) const {
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

Name PdfObjectParser::read_name() const {
  std::stringstream ss;
  read_name(ss);
  return ss.str();
}

bool PdfObjectParser::peek_null() const {
  int_type c = sb().sgetc();
  return c != eof && (c == 'n' || c == 'N');
}

void PdfObjectParser::read_null() const {
  char tmp[4];
  if (sb().sgetn(tmp, 4) != 4) {
    throw std::runtime_error("unexpected stream exhaust");
  }
  // TODO check ignorecase
}

bool PdfObjectParser::peek_boolean() const {
  int_type c = sb().sgetc();
  return c != eof && (c == 't' || c == 'T' || c == 'f' || c == 'F');
}

Boolean PdfObjectParser::read_boolean() const {
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

bool PdfObjectParser::peek_string() const {
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

void PdfObjectParser::read_string(std::ostream &out) const {
  int_type c = sb().sbumpc();

  if (c == '(') {
    while (true) {
      c = sb().sbumpc();

      if (c == eof) {
        in().setstate(std::ios::eofbit);
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
      c = sb().sbumpc();

      if (c == eof) {
        in().setstate(std::ios::eofbit);
        throw std::runtime_error("unexpected stream exhaust");
      }
      if (c == '>') {
        return;
      }

      int_type c2 = sb().sbumpc();

      if (c2 == eof) {
        in().setstate(std::ios::eofbit);
        throw std::runtime_error("unexpected stream exhaust");
      }

      out.put(two_hex_to_char(c, c2));
    }
  }

  throw std::runtime_error("unexpected starting character");
}

String PdfObjectParser::read_string() const {
  std::stringstream ss;
  read_string(ss);
  return ss.str();
}

bool PdfObjectParser::peek_array() const {
  int_type c = sb().sgetc();
  return c != eof && c == '[';
}

Array PdfObjectParser::read_array() const {
  Array result;

  if (sb().sbumpc() != '[') {
    throw std::runtime_error("unexpected character");
  }
  skip_whitespace();

  while (true) {
    if (int_type c = sb().sgetc(); c == ']') {
      sb().sbumpc();
      return result;
    }
    Object value = read_object();
    skip_whitespace();

    result.emplace_back(std::move(value));

    if (int_type c = sb().sgetc(); c == 'R') {
      sb().sbumpc();
      skip_whitespace();

      auto gen = std::any_cast<Integer>(result.back());
      result.pop_back();
      auto id = std::any_cast<Integer>(result.back());
      result.pop_back();
      result.emplace_back(ObjectReference{id, gen});
    }
  }
}

bool PdfObjectParser::peek_dictionary() const {
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

Dictionary PdfObjectParser::read_dictionary() const {
  Dictionary result;

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
      return result;
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

      value = ObjectReference{std::any_cast<Integer>(value), gen};
    }

    result.emplace(std::move(name), std::move(value));
  }
}

Object PdfObjectParser::read_object() const {
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
    return read_string();
  }
  if (peek_array()) {
    return read_array();
  }
  if (peek_dictionary()) {
    return read_dictionary();
  }

  throw std::runtime_error("unknown object");
}

ObjectReference PdfObjectParser::read_object_reference() const {
  UnsignedInteger id = read_unsigned_integer();
  skip_whitespace();
  UnsignedInteger gen = read_unsigned_integer();
  skip_whitespace();

  if (sb().sbumpc() != 'R') {
    throw std::runtime_error("unexpected character");
  }

  return {id, gen};
}

PdfFileParser::PdfFileParser(std::istream &in) : m_parser(in) {}

IndirectObject
PdfFileParser::read_indirect_object(std::optional<std::string> head) const {
  if (!head) {
    head = m_parser.read_line();
  }

  IndirectObject result;

  {
    std::istringstream ss(*head);
    PdfObjectParser head_parser(ss);
    result.reference.first = head_parser.read_unsigned_integer();
    head_parser.skip_whitespace();
    result.reference.second = head_parser.read_unsigned_integer();
  }

  result.object = m_parser.read_object();
  m_parser.skip_line();

  auto next = m_parser.read_line();

  if (next == "endobj") {
    return result;
  }
  if (next == "stream") {
    result.has_stream = true;
    std::string stream;

    // TODO improve poor solution
    while (true) {
      std::string line = util::stream::read_until(m_parser.in(), '\n', true);
      if (line == "endstream\n") {
        stream.pop_back();
        break;
      }
      stream += line;
    }

    result.stream = std::move(stream);

    if (std::string line = m_parser.read_line(); line != "endobj") {
      throw std::runtime_error("expected endobj");
    }

    return result;
  }

  throw std::runtime_error("expected stream");
}

Trailer PdfFileParser::read_trailer(std::optional<std::string> head) const {
  if (!head) {
    m_parser.skip_line();
  }

  Trailer result;

  result.trailer = m_parser.read_dictionary();
  m_parser.skip_line();

  return result;
}

Xref PdfFileParser::read_xref(std::optional<std::string> head) const {
  if (!head) {
    m_parser.skip_line();
  }

  Xref result;

  while (true) {
    if (!m_parser.peek_number()) {
      return result;
    }

    std::uint32_t subsection_id = m_parser.read_integer();
    m_parser.skip_whitespace();
    std::uint32_t subsection_size = m_parser.read_integer();
    m_parser.skip_line();

    std::vector<std::string> subsection;
    for (std::uint32_t i = 0; i < subsection_size; ++i) {
      std::string subsection_entry = m_parser.read_line();
      subsection.emplace_back(std::move(subsection_entry));
    }
    result.table.emplace(subsection_id, std::move(subsection));
  }
}

StartXref PdfFileParser::read_startxref(std::optional<std::string> head) const {
  if (!head) {
    m_parser.skip_line();
  }

  StartXref result;

  result.start = m_parser.peek_number();
  m_parser.skip_line();

  return result;
}

void PdfFileParser::read_header() const {
  std::string header1 = m_parser.read_line();
  std::string header2 = m_parser.read_line();

  if (!util::string::starts_with(header1, "%PDF-")) {
    throw std::runtime_error("illegal header");
  }
}

Entry PdfFileParser::read_entry() const {
  m_parser.skip_whitespace();
  std::string entry_header = m_parser.read_line();

  if (util::string::ends_with(entry_header, "obj")) {
    return read_indirect_object(std::move(entry_header));
  }
  if (entry_header == "xref") {
    return read_xref(std::move(entry_header));
  }
  if (entry_header == "trailer") {
    return read_trailer(std::move(entry_header));
  }
  if (entry_header == "startxref") {
    return read_startxref(std::move(entry_header));
  }
  if (entry_header == "%%EOF") {
    return Eof{};
  }

  return {};
}

} // namespace odr::internal::pdf
