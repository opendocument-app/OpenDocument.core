#include <odr/internal/pdf/pdf_cmap_parser.hpp>

#include <odr/internal/pdf/pdf_cmap.hpp>
#include <odr/internal/util/byte_util.hpp>

#include <iostream>

namespace odr::internal::pdf {

using char_type = std::streambuf::char_type;
using int_type = std::streambuf::int_type;
static constexpr int_type eof = std::streambuf::traits_type::eof();

CMapParser::CMapParser(std::istream &in) : m_parser(in) {}

std::istream &CMapParser::in() const { return m_parser.in(); }

std::streambuf &CMapParser::sb() const { return m_parser.sb(); }

const ObjectParser &CMapParser::parser() const { return m_parser; }

std::variant<Object, std::string> CMapParser::read_token() const {
  if (m_parser.peek_number()) {
    return std::visit([](auto n) { return Object(n); },
                      m_parser.read_integer_or_real());
  }
  if (m_parser.peek_string()) {
    return std::visit([](auto s) { return Object(std::move(s)); },
                      m_parser.read_string());
  }
  if (m_parser.peek_name()) {
    return Object(m_parser.read_name());
  }
  if (m_parser.peek_dictionary()) {
    return Object(m_parser.read_dictionary());
  }

  std::string token;
  while (true) {
    const int_type i = m_parser.geti();
    if (i == eof) {
      return token;
    }
    const auto c = static_cast<char_type>(i);
    if (ObjectParser::is_whitespace(c)) {
      return token;
    }
    m_parser.bumpc();
    token += c;
  }
}

void CMapParser::read_codespacerange(const std::uint32_t n,
                                     const CMap &cmap) const {
  (void)cmap;

  m_parser.skip_whitespace();
  for (std::uint32_t i = 0; i < n; ++i) {
    auto from_glyph = m_parser.read_object();
    m_parser.skip_whitespace();
    auto to_glyph = m_parser.read_object();
    m_parser.skip_whitespace();

    // TODO
  }
}

void CMapParser::read_bfchar(const std::uint32_t n, CMap &cmap) const {
  m_parser.skip_whitespace();
  for (std::uint32_t i = 0; i < n; ++i) {
    std::string glyph = m_parser.read_object().as_string();
    m_parser.skip_whitespace();
    std::string unicode = m_parser.read_object().as_string();
    m_parser.skip_whitespace();

    util::reverse_bytes(reinterpret_cast<char16_t *>(unicode.data()),
                        unicode.size() / 2);
    std::u16string_view unicode16(
        reinterpret_cast<const char16_t *>(unicode.data()), unicode.size() / 2);

    if (glyph.length() != 1) {
      std::cerr << "unexpected glyph length" << std::endl;
    }
    if (unicode16.length() != 1) {
      std::cerr << "unexpected unicode length" << std::endl;
    }

    cmap.map_bfchar(glyph[0], unicode16[0]);
  }
}

void CMapParser::read_bfrange(const std::uint32_t n, const CMap &cmap) const {
  (void)cmap;

  m_parser.skip_whitespace();
  for (std::uint32_t i = 0; i < n; ++i) {
    auto from_glyph = m_parser.read_object();
    m_parser.skip_whitespace();
    auto to_glyph = m_parser.read_object();
    m_parser.skip_whitespace();
    auto unicode = m_parser.read_object();
    m_parser.skip_whitespace();

    // TODO
  }
}

CMap CMapParser::parse_cmap() const {
  CMap cmap;

  std::uint32_t last_int{};

  m_parser.skip_whitespace();
  while (true) {
    Token token = read_token();
    if (in().eof()) {
      break;
    }
    m_parser.skip_whitespace();

    if (std::holds_alternative<Object>(token)) {
      if (const Object &object = std::get<Object>(token); object.is_integer()) {
        last_int = object.as_integer();
      }
    } else if (std::holds_alternative<std::string>(token)) {
      if (const std::string &command = std::get<std::string>(token);
          command == "begincodespacerange") {
        read_codespacerange(last_int, cmap);
      } else if (command == "beginbfchar") {
        read_bfchar(last_int, cmap);
      } else if (command == "beginbfrange") {
        read_bfrange(last_int, cmap);
      }
    }
  }

  return cmap;
}

} // namespace odr::internal::pdf
