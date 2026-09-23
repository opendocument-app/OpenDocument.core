#include <odr/internal/pdf/pdf_cmap.hpp>

#include <odr/internal/util/byte_string.hpp>
#include <odr/internal/util/string_util.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>

namespace odr::internal::pdf {

void CMap::add_codespace_range(std::string low_code, std::string high_code) {
  m_codespace_ranges.push_back({std::move(low_code), std::move(high_code)});
}

void CMap::map_single(std::string code, std::u16string unicode) {
  m_map[std::move(code)] = std::move(unicode);
}

void CMap::map_range(const std::uint32_t low, const std::uint32_t high,
                     const std::size_t width, std::u16string unicode) {
  m_ranges.push_back({low, high, width, std::move(unicode)});
}

std::optional<std::u16string>
CMap::unicode_for_code(const std::string &code) const {
  if (const auto it = m_map.find(code); it != m_map.end()) {
    return it->second;
  }
  const std::uint32_t value =
      util::byte_string::read_uint_be(code, code.size());
  for (auto it = m_ranges.rbegin(); it != m_ranges.rend(); ++it) {
    if (it->width == code.size() && value >= it->low && value <= it->high) {
      std::u16string unicode = it->unicode;
      if (!unicode.empty()) {
        unicode.back() =
            static_cast<char16_t>(unicode.back() + (value - it->low));
      }
      return unicode;
    }
  }
  return std::nullopt;
}

void CMap::map_cid_char(std::string code, const std::uint32_t cid) {
  m_cid_chars[std::move(code)] = cid;
}

void CMap::add_cid_range(const std::uint32_t low, const std::uint32_t high,
                         const std::uint32_t base_cid,
                         const std::size_t width) {
  m_cid_ranges.push_back({low, high, base_cid, width});
}

std::optional<std::uint32_t>
CMap::cid_for_code(const std::string_view code) const {
  if (const auto it = m_cid_chars.find(std::string(code));
      it != m_cid_chars.end()) {
    return it->second;
  }
  const std::uint32_t value =
      util::byte_string::read_uint_be(code, code.size());
  for (const CidRange &range : m_cid_ranges) {
    if (range.width == code.size() && value >= range.low &&
        value <= range.high) {
      return range.base_cid + (value - range.low);
    }
  }
  return std::nullopt;
}

std::size_t CMap::code_width(const std::uint8_t first) const {
  for (const CodespaceRange &range : m_codespace_ranges) {
    if (first >= static_cast<std::uint8_t>(range.low.front()) &&
        first <= static_cast<std::uint8_t>(range.high.front())) {
      return range.low.size();
    }
  }
  // No codespace range declares this code; assume single-byte (the historic
  // behaviour, which is also correct for the simple-font ToUnicode CMaps that
  // omit the codespace declaration).
  return 1;
}

std::size_t CMap::code_length(const std::string &codes,
                              const std::size_t pos) const {
  return code_width(static_cast<std::uint8_t>(codes[pos]));
}

std::string CMap::translate_string(const std::string &codes,
                                   const bool single_byte_codes) const {
  std::u16string result;

  std::size_t pos = 0;
  while (pos < codes.size()) {
    const std::size_t width =
        single_byte_codes
            ? 1
            : std::min(code_length(codes, pos), codes.size() - pos);
    const std::string code = codes.substr(pos, width);
    pos += width;

    if (const std::optional<std::u16string> unicode = unicode_for_code(code)) {
      result += *unicode;
      continue;
    }

    // Only for an imposed width — a declared mixed codespace keeps `<20>` and
    // `<0020>` distinct.
    if (single_byte_codes) {
      if (const std::optional<std::u16string> unicode =
              unicode_for_code(std::string(1, '\0') + code)) {
        result += *unicode;
        continue;
      }
    }

    // Unknown code: fall back to its numeric value as a single UTF-16 unit
    // (identity for single-byte codes). These "no Unicode" runs are left for
    // later re-encoding.
    result += static_cast<char16_t>(
        util::byte_string::read_uint_be(code, code.size()));
  }

  return util::string::u16string_to_string(result);
}

} // namespace odr::internal::pdf
