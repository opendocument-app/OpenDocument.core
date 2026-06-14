#include <odr/internal/pdf/pdf_cmap_parser.hpp>

#include <odr/internal/pdf/pdf_cmap.hpp>

namespace odr::internal::pdf {

using char_type = std::streambuf::char_type;
using int_type = std::streambuf::int_type;
static constexpr int_type eof = std::streambuf::traits_type::eof();

namespace {

// A code (or its destination) arrives as a big-endian byte string, e.g. from a
// hex string `<0041>`.
std::uint32_t code_to_uint(const std::string &code) {
  std::uint32_t value = 0;
  for (const char c : code) {
    value = (value << 8) | static_cast<unsigned char>(c);
  }
  return value;
}

std::string uint_to_code(std::uint32_t value, const std::size_t width) {
  std::string code(width, '\0');
  for (std::size_t i = 0; i < width; ++i) {
    code[width - 1 - i] = static_cast<char>(value & 0xff);
    value >>= 8;
  }
  return code;
}

// A `bfchar`/`bfrange` destination is a UTF-16BE byte string.
std::u16string utf16be_to_u16string(const std::string &bytes) {
  std::u16string result;
  result.reserve(bytes.size() / 2);
  for (std::size_t i = 0; i + 1 < bytes.size(); i += 2) {
    result.push_back(
        static_cast<char16_t>((static_cast<unsigned char>(bytes[i]) << 8) |
                              static_cast<unsigned char>(bytes[i + 1])));
  }
  return result;
}

} // namespace

CMapParser::CMapParser(std::istream &in) : m_parser(in) {}

std::istream &CMapParser::in() { return m_parser.in(); }

std::streambuf &CMapParser::sb() { return m_parser.sb(); }

const ObjectParser &CMapParser::parser() { return m_parser; }

std::variant<Object, std::string> CMapParser::read_token() {
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

void CMapParser::read_codespacerange(const std::uint32_t n, CMap &cmap) {
  m_parser.skip_whitespace();
  for (std::uint32_t i = 0; i < n; ++i) {
    std::string low = m_parser.read_object().as_string();
    m_parser.skip_whitespace();
    std::string high = m_parser.read_object().as_string();
    m_parser.skip_whitespace();

    cmap.add_codespace_range(std::move(low), std::move(high));
  }
}

void CMapParser::read_bfchar(const std::uint32_t n, CMap &cmap) {
  m_parser.skip_whitespace();
  for (std::uint32_t i = 0; i < n; ++i) {
    std::string code = m_parser.read_object().as_string();
    m_parser.skip_whitespace();
    std::u16string unicode =
        utf16be_to_u16string(m_parser.read_object().as_string());
    m_parser.skip_whitespace();

    cmap.map_single(std::move(code), std::move(unicode));
  }
}

void CMapParser::read_bfrange(const std::uint32_t n, CMap &cmap) {
  m_parser.skip_whitespace();
  for (std::uint32_t i = 0; i < n; ++i) {
    std::string low = m_parser.read_object().as_string();
    m_parser.skip_whitespace();
    std::string high = m_parser.read_object().as_string();
    m_parser.skip_whitespace();
    Object destination = m_parser.read_object();
    m_parser.skip_whitespace();

    const std::size_t width = low.size();
    const std::uint32_t low_code = code_to_uint(low);
    const std::uint32_t high_code = code_to_uint(high);

    if (destination.is_array()) {
      // `<lo> <hi> [ <dst0> <dst1> … ]`: one destination per code.
      std::uint32_t code = low_code;
      for (const Object &element : destination.as_array()) {
        if (code > high_code) {
          break;
        }
        cmap.map_single(uint_to_code(code, width),
                        utf16be_to_u16string(element.as_string()));
        ++code;
      }
    } else if (destination.is_string()) {
      // `<lo> <hi> <dst>`: the destination's last UTF-16 unit increments across
      // the code range (7.9.3).
      std::u16string unicode = utf16be_to_u16string(destination.as_string());
      for (std::uint32_t code = low_code;; ++code) {
        cmap.map_single(uint_to_code(code, width), unicode);
        if (code == high_code) {
          break;
        }
        if (!unicode.empty()) {
          ++unicode.back();
        }
      }
    }
  }
}

CMap CMapParser::parse_cmap() {
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
