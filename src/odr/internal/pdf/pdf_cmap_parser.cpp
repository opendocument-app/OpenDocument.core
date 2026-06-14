#include <odr/internal/pdf/pdf_cmap_parser.hpp>

#include <odr/internal/pdf/pdf_cmap.hpp>

#include <odr/logger.hpp>

#include <sstream>
#include <stdexcept>

namespace odr::internal::pdf {

using char_type = std::streambuf::char_type;
using int_type = std::streambuf::int_type;
static constexpr int_type eof = std::streambuf::traits_type::eof();

namespace {

// PDF character codes are 1–4 bytes wide (PDF 32000-1 §9.7.6.2); that bound is
// what lets a code be packed losslessly into a `std::uint32_t`. The call sites
// validate widths and skip out-of-spec entries, so in practice the guards in
// the helpers below never fire — they enforce the invariant defensively so a
// stray caller fails loudly rather than silently corrupting the mapping.
constexpr std::size_t max_code_width = 4;

bool valid_code_width(const std::size_t width) {
  return width >= 1 && width <= max_code_width;
}

/// A code (or its destination) arrives as a big-endian byte string, e.g. from a
/// hex string `<0041>`.
std::uint32_t code_to_uint(const std::string &code) {
  if (!valid_code_width(code.size())) {
    throw std::invalid_argument("pdf: CMap code width out of range");
  }
  std::uint32_t value = 0;
  for (const char c : code) {
    value = (value << 8) | static_cast<std::uint8_t>(c);
  }
  return value;
}

std::string uint_to_code(std::uint32_t value, const std::size_t width) {
  if (!valid_code_width(width)) {
    throw std::invalid_argument("pdf: CMap code width out of range");
  }
  std::string code(width, '\0');
  for (std::size_t i = 0; i < width; ++i) {
    code[width - 1 - i] = static_cast<char>(value & 0xff);
    value >>= 8;
  }
  return code;
}

/// A `bfchar`/`bfrange` destination is a UTF-16BE byte string, hence an even
/// number of bytes.
std::u16string utf16be_to_u16string(const std::string &bytes) {
  if (bytes.size() % 2 != 0) {
    throw std::invalid_argument("pdf: UTF-16BE byte string of odd length");
  }
  std::u16string result;
  result.reserve(bytes.size() / 2);
  for (std::size_t i = 0; i + 1 < bytes.size(); i += 2) {
    result.push_back(
        static_cast<char16_t>((static_cast<std::uint8_t>(bytes[i]) << 8) |
                              static_cast<std::uint8_t>(bytes[i + 1])));
  }
  return result;
}

} // namespace

CMapParser::CMapParser(std::istream &in, const Logger &logger)
    : m_parser(in), m_logger{&logger} {}

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

    if (low.size() != high.size() || !valid_code_width(low.size())) {
      ODR_WARNING(*m_logger, "pdf: skipping out-of-spec codespace range (low "
                                 << low.size() << " bytes, high " << high.size()
                                 << " bytes)");
      continue; // skip an out-of-spec range, keep parsing the rest
    }
    cmap.add_codespace_range(std::move(low), std::move(high));
  }
}

void CMapParser::read_bfchar(const std::uint32_t n, CMap &cmap) {
  m_parser.skip_whitespace();
  for (std::uint32_t i = 0; i < n; ++i) {
    std::string code = m_parser.read_object().as_string();
    m_parser.skip_whitespace();
    std::string destination = m_parser.read_object().as_string();
    m_parser.skip_whitespace();

    if (!valid_code_width(code.size()) || destination.size() % 2 != 0) {
      ODR_WARNING(*m_logger, "pdf: skipping malformed bfchar entry (code "
                                 << code.size() << " bytes, destination "
                                 << destination.size() << " bytes)");
      continue; // skip a malformed mapping, keep the rest of the CMap
    }
    cmap.map_single(std::move(code), utf16be_to_u16string(destination));
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

    if (low.size() != high.size() || !valid_code_width(low.size())) {
      ODR_WARNING(*m_logger, "pdf: skipping out-of-spec bfrange (low "
                                 << low.size() << " bytes, high " << high.size()
                                 << " bytes)");
      continue; // skip an out-of-spec range, keep parsing the rest
    }
    const std::size_t width = low.size();
    const std::uint32_t low_code = code_to_uint(low);
    const std::uint32_t high_code = code_to_uint(high);
    if (high_code < low_code) {
      ODR_WARNING(*m_logger, "pdf: skipping reversed bfrange (low 0x"
                                 << std::hex << low_code << ", high 0x"
                                 << high_code << ")");
      continue; // a reversed range would otherwise wrap `code` below
    }

    if (destination.is_array()) {
      // `<lo> <hi> [ <dst0> <dst1> … ]`: one destination per code.
      std::uint32_t code = low_code;
      for (const Object &element : destination.as_array()) {
        if (code > high_code) {
          break;
        }
        if (element.is_string()) {
          if (const std::string &dst = element.as_string();
              dst.size() % 2 == 0) {
            cmap.map_single(uint_to_code(code, width),
                            utf16be_to_u16string(dst));
          } else {
            ODR_WARNING(*m_logger,
                        "pdf: skipping odd-length bfrange array destination ("
                            << dst.size() << " bytes)");
          }
        } else {
          ODR_WARNING(*m_logger,
                      "pdf: skipping non-string bfrange array destination");
        }
        ++code;
      }
    } else if (destination.is_string()) {
      // `<lo> <hi> <dst>`: the destination's last UTF-16 unit increments across
      // the code range (7.9.3).
      const std::string &dst = destination.as_string();
      if (dst.size() % 2 != 0) {
        ODR_WARNING(*m_logger,
                    "pdf: skipping bfrange with odd-length destination ("
                        << dst.size() << " bytes)");
        continue;
      }
      std::u16string unicode = utf16be_to_u16string(dst);
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
