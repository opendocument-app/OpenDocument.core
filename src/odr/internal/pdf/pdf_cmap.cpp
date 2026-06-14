#include <odr/internal/pdf/pdf_cmap.hpp>

#include <odr/internal/util/string_util.hpp>

#include <algorithm>

namespace odr::internal::pdf {

void CMap::add_codespace_range(std::string low_code, std::string high_code) {
  m_codespace_ranges.push_back({std::move(low_code), std::move(high_code)});
}

void CMap::map_single(std::string code, std::u16string unicode) {
  m_map[std::move(code)] = std::move(unicode);
}

std::size_t CMap::code_length(const std::string &codes,
                              const std::size_t pos) const {
  const auto first = static_cast<unsigned char>(codes[pos]);
  for (const CodespaceRange &range : m_codespace_ranges) {
    if (first >= static_cast<unsigned char>(range.low.front()) &&
        first <= static_cast<unsigned char>(range.high.front())) {
      return range.low.size();
    }
  }
  // No codespace range declares this code; assume single-byte (the historic
  // behaviour, which is also correct for the simple-font ToUnicode CMaps that
  // omit the codespace declaration).
  return 1;
}

std::string CMap::translate_string(const std::string &codes) const {
  std::u16string result;

  std::size_t pos = 0;
  while (pos < codes.size()) {
    const std::size_t width =
        std::min(code_length(codes, pos), codes.size() - pos);
    const std::string code = codes.substr(pos, width);
    pos += width;

    if (const auto it = m_map.find(code); it != m_map.end()) {
      result += it->second;
      continue;
    }

    // Unknown code: fall back to its numeric value as a single UTF-16 unit
    // (identity for single-byte codes). Stage 1.5 will refine the handling of
    // these "no Unicode" runs.
    std::uint32_t value = 0;
    for (const char c : code) {
      value = (value << 8) | static_cast<unsigned char>(c);
    }
    result += static_cast<char16_t>(value);
  }

  return util::string::u16string_to_string(result);
}

} // namespace odr::internal::pdf
