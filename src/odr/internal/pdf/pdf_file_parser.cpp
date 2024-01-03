#include <odr/internal/pdf/pdf_file_parser.hpp>

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

ObjectParser::ObjectParser(std::istream &in)
    : m_in{&in}, m_se(in, true), m_sb{in.rdbuf()} {}

std::istream &ObjectParser::in() const { return *m_in; }

std::streambuf &ObjectParser::sb() const { return *m_sb; }

void ObjectParser::skip_whitespace() const {
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

void ObjectParser::skip_line() const { read_line(); }

std::string ObjectParser::read_line() const {
  return util::stream::read_until(in(), '\n', false);
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

IntegerOrReal ObjectParser::read_integer_or_real() const {
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
  // TODO check ignorecase
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

void ObjectParser::read_string(std::ostream &out) const {
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

String ObjectParser::read_string() const {
  std::stringstream ss;
  read_string(ss);
  return ss.str();
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

      auto gen = result.back().as_integer();
      result.pop_back();
      auto id = result.back().as_integer();
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

      value = ObjectReference{value.as_integer(), gen};
    }

    result.emplace(std::move(name), std::move(value));
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

FileParser::FileParser(std::istream &in) : m_parser(in) {}

std::istream &FileParser::in() const { return m_parser.in(); }

std::streambuf &FileParser::sb() const { return m_parser.sb(); }

const ObjectParser &FileParser::parser() const { return m_parser; }

IndirectObject FileParser::read_indirect_object() const {
  IndirectObject result;

  result.reference.first = m_parser.read_unsigned_integer();
  m_parser.skip_whitespace();
  result.reference.second = m_parser.read_unsigned_integer();
  m_parser.skip_whitespace();
  if (std::string line = m_parser.read_line(); line != "obj") {
    throw std::runtime_error("expected obj");
  }

  result.object = m_parser.read_object();
  m_parser.skip_line();

  auto next = m_parser.read_line();

  if (next == "endobj") {
    return result;
  }
  if (next == "stream") {
    result.has_stream = true;
    result.stream_position = in().tellg();
    std::string stream;

    // TODO improve poor solution
    while (true) {
      std::string line = util::stream::read_until(in(), '\n', true);
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

Trailer FileParser::read_trailer() const {
  if (std::string line = m_parser.read_line(); line != "trailer") {
    throw std::runtime_error("expected trailer");
  }

  Trailer result;

  result.dictionary = m_parser.read_dictionary();
  result.size = result.dictionary["Size"].as_integer();
  result.root_reference = result.dictionary["Root"].as_reference();
  result.info_reference = result.dictionary["Info"].as_reference();

  m_parser.skip_line();

  return result;
}

Xref FileParser::read_xref() const {
  if (std::string line = m_parser.read_line(); line != "xref") {
    throw std::runtime_error("expected xref");
  }

  Xref result;

  while (true) {
    if (!m_parser.peek_number()) {
      return result;
    }

    std::uint32_t first_id = m_parser.read_integer();
    m_parser.skip_whitespace();
    std::uint32_t entry_count = m_parser.read_integer();
    m_parser.skip_line();

    for (std::uint32_t i = 0; i < entry_count; ++i) {
      Xref::Entry entry;

      entry.position = m_parser.read_unsigned_integer();
      m_parser.skip_whitespace();
      entry.generation = m_parser.read_unsigned_integer();
      m_parser.skip_whitespace();
      entry.in_use = m_parser.read_line().at(0) == 'n';

      result.table.emplace(first_id + i, std::move(entry));
    }
  }
}

StartXref FileParser::read_start_xref() const {
  if (std::string line = m_parser.read_line(); line != "startxref") {
    throw std::runtime_error("expected startxref");
  }

  StartXref result;

  result.start = m_parser.read_unsigned_integer();
  m_parser.skip_line();

  return result;
}

void FileParser::read_header() const {
  std::string header1 = m_parser.read_line();
  std::string header2 = m_parser.read_line();

  if (!util::string::starts_with(header1, "%PDF-")) {
    throw std::runtime_error("illegal header");
  }
}

Entry FileParser::read_entry() const {
  m_parser.skip_whitespace();
  std::uint32_t position = in().tellg();
  std::string entry_header = m_parser.read_line();
  in().seekg(position);

  if (util::string::ends_with(entry_header, "obj")) {
    return {read_indirect_object(), position};
  }
  if (entry_header == "xref") {
    return {read_xref(), position};
  }
  if (entry_header == "trailer") {
    return {read_trailer(), position};
  }
  if (entry_header == "startxref") {
    return {read_start_xref(), position};
  }
  if (entry_header == "%PDF-") {
    read_header();
    return {Header{}, position};
  }
  if (entry_header == "%%EOF") {
    return {Eof{}, position};
  }

  throw std::runtime_error("unknown entry");
}

void FileParser::seek_start_xref(std::uint32_t margin) const {
  in().seekg(0, std::ios::end);
  std::int64_t size = in().tellg();
  in().seekg(std::min(0l, size - margin), std::ios::beg);

  while (!m_parser.in().eof()) {
    std::uint32_t position = m_parser.in().tellg();
    std::string line = m_parser.read_line();
    if (line == "startxref") {
      m_parser.in().seekg(position);
      return;
    }
  }

  throw std::runtime_error("unexpected stream exhaust");
}

} // namespace odr::internal::pdf
