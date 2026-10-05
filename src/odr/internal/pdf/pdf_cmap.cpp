#include <odr/internal/pdf/pdf_cmap.hpp>

#include <odr/internal/util/byte_string.hpp>
#include <odr/internal/util/string_util.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <ranges>
#include <stdexcept>

namespace odr::internal {

bool pdf::matches_codespace(const std::string_view bytes,
                            const std::uint32_t low, const std::uint32_t high,
                            const std::size_t width) {
  if (width == 0 || width > 4 || bytes.size() < width) {
    return false;
  }
  for (std::size_t i = 0; i < width; ++i) {
    const std::size_t shift = 8 * (width - 1 - i);
    const auto value = static_cast<std::uint8_t>(bytes[i]);
    if (value < static_cast<std::uint8_t>(low >> shift) ||
        value > static_cast<std::uint8_t>(high >> shift)) {
      return false;
    }
  }
  return true;
}

} // namespace odr::internal

namespace odr::internal::pdf {

void CMap::add_codespace_range(const std::string_view low_code,
                               const std::string_view high_code) {
  if (low_code.empty() || low_code.size() > 4 ||
      low_code.size() != high_code.size()) {
    throw std::invalid_argument("pdf: invalid codespace width");
  }
  m_codespace_ranges.push_back(
      {util::byte_string::read_uint_be(low_code, low_code.size()),
       util::byte_string::read_uint_be(high_code, high_code.size()),
       low_code.size()});
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
  for (const CidRange &range : m_cid_ranges | std::views::reverse) {
    if (range.width == code.size() && value >= range.low &&
        value <= range.high) {
      return range.base_cid + (value - range.low);
    }
  }
  return std::nullopt;
}

std::size_t CMap::code_width(const std::string_view bytes) const {
  std::size_t width = 5;
  for (const CodespaceRange &range : m_codespace_ranges) {
    if (range.width < width &&
        matches_codespace(bytes, range.low, range.high, range.width)) {
      width = range.width;
    }
  }
  return width == 5 ? 1 : width;
}

std::string CMap::translate_string(const std::string &codes,
                                   const bool single_byte_codes) const {
  std::u16string result;

  std::size_t pos = 0;
  while (pos < codes.size()) {
    const std::size_t width =
        single_byte_codes ? 1 : code_width(std::string_view(codes).substr(pos));
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
