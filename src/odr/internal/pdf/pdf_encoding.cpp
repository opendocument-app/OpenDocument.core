#include <odr/internal/pdf/pdf_encoding.hpp>

#include <odr/internal/pdf/pdf_encoding_data.hpp>
#include <odr/internal/util/string_util.hpp>

#include <algorithm>
#include <charconv>

namespace odr::internal::pdf {

const std::array<std::string_view, 256> &
base_encoding_table(const BaseEncoding encoding) {
  switch (encoding) {
  case BaseEncoding::standard:
    return encoding_data::standard_encoding;
  case BaseEncoding::win_ansi:
    return encoding_data::win_ansi_encoding;
  case BaseEncoding::mac_roman:
    return encoding_data::mac_roman_encoding;
  case BaseEncoding::pdf_doc:
    return encoding_data::pdf_doc_encoding;
  }
  return encoding_data::standard_encoding;
}

std::optional<BaseEncoding>
base_encoding_from_name(const std::string_view name) {
  if (name == "StandardEncoding") {
    return BaseEncoding::standard;
  }
  if (name == "WinAnsiEncoding") {
    return BaseEncoding::win_ansi;
  }
  if (name == "MacRomanEncoding") {
    return BaseEncoding::mac_roman;
  }
  if (name == "PDFDocEncoding") {
    return BaseEncoding::pdf_doc;
  }
  return std::nullopt;
}

namespace {

// Parse the algorithmic glyph-name forms: `uniXXXX` (one or more UTF-16 code
// units, 4 hex digits each) and `uXXXXXX` (a single 4-6 hex-digit scalar). See
// the AGL specification, "Step 2". Returns empty on any deviation.
std::u16string algorithmic_glyph_name_to_unicode(const std::string_view name) {
  const auto parse_hex =
      [](const std::string_view hex) -> std::optional<std::uint32_t> {
    std::uint32_t value = 0;
    const auto *const begin = hex.data();
    const auto *const end = begin + hex.size();
    const auto [ptr, ec] = std::from_chars(begin, end, value, 16);
    if (ec != std::errc() || ptr != end) {
      return std::nullopt;
    }
    return value;
  };

  if (name.starts_with("uni") && (name.size() - 3) % 4 == 0 &&
      name.size() > 3) {
    std::u16string result;
    for (std::size_t pos = 3; pos < name.size(); pos += 4) {
      const auto unit = parse_hex(name.substr(pos, 4));
      if (!unit) {
        return {};
      }
      result.push_back(static_cast<char16_t>(*unit));
    }
    return result;
  }

  if (name.starts_with("u") && name.size() >= 5 && name.size() <= 7) {
    const auto scalar = parse_hex(name.substr(1));
    if (!scalar || *scalar > 0x10FFFF ||
        (*scalar >= 0xD800 && *scalar <= 0xDFFF)) {
      return {};
    }
    if (*scalar <= 0xFFFF) {
      return std::u16string(1, static_cast<char16_t>(*scalar));
    }
    const std::uint32_t v = *scalar - 0x10000;
    return {static_cast<char16_t>(0xD800 + (v >> 10)),
            static_cast<char16_t>(0xDC00 + (v & 0x3FF))};
  }

  return {};
}

} // namespace

std::u16string glyph_name_to_unicode(std::string_view glyph_name) {
  // A composite name (`a.alt`, `f_i`) is out of scope for this stage; use the
  // part before the first '.' as the AGL spec prescribes, leave '_' joins to a
  // later refinement.
  if (const auto dot = glyph_name.find('.'); dot != std::string_view::npos) {
    glyph_name = glyph_name.substr(0, dot);
  }

  const auto &agl = encoding_data::adobe_glyph_list;
  const auto it =
      std::lower_bound(agl.begin(), agl.end(), glyph_name,
                       [](const auto &entry, const std::string_view name) {
                         return entry.first < name;
                       });
  if (it != agl.end() && it->first == glyph_name) {
    return std::u16string(it->second);
  }

  return algorithmic_glyph_name_to_unicode(glyph_name);
}

Encoding::Encoding(const BaseEncoding base)
    : m_base{base_encoding_table(base)} {}

void Encoding::set_difference(const std::uint8_t code, std::string glyph_name) {
  m_differences[code] = std::move(glyph_name);
}

std::string_view Encoding::glyph_name(const std::uint8_t code) const {
  if (const auto it = m_differences.find(code); it != m_differences.end()) {
    return it->second;
  }
  return m_base[code];
}

std::string Encoding::translate_string(const std::string &codes) const {
  std::u16string result;
  for (const char c : codes) {
    const auto code = static_cast<std::uint8_t>(c);
    result += glyph_name_to_unicode(glyph_name(code));
  }
  return util::string::u16string_to_string(result);
}

} // namespace odr::internal::pdf
