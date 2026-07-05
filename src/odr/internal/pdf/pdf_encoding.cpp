#include <odr/internal/pdf/pdf_encoding.hpp>

#include <odr/internal/pdf/pdf_encoding_data.hpp>
#include <odr/internal/util/string_util.hpp>

#include <algorithm>
#include <charconv>
#include <stdexcept>

namespace odr::internal {

char32_t pdf::pdf_doc_encoding_to_unicode(const std::uint8_t byte) {
  switch (byte) {
  case 0x18:
    return U'˘'; // breve
  case 0x19:
    return U'ˇ'; // caron
  case 0x1A:
    return U'ˆ'; // circumflex
  case 0x1B:
    return U'˙'; // dot above
  case 0x1C:
    return U'˝'; // double acute
  case 0x1D:
    return U'˛'; // ogonek
  case 0x1E:
    return U'˚'; // ring above
  case 0x1F:
    return U'˜'; // small tilde
  case 0x80:
    return U'•'; // bullet
  case 0x81:
    return U'†'; // dagger
  case 0x82:
    return U'‡'; // double dagger
  case 0x83:
    return U'…'; // ellipsis
  case 0x84:
    return U'—'; // em dash
  case 0x85:
    return U'–'; // en dash
  case 0x86:
    return U'ƒ'; // florin
  case 0x87:
    return U'⁄'; // fraction slash
  case 0x88:
    return U'‹'; // single left angle quote
  case 0x89:
    return U'›'; // single right angle quote
  case 0x8A:
    return U'−'; // minus
  case 0x8B:
    return U'‰'; // per mille
  case 0x8C:
    return U'„'; // double low-9 quote
  case 0x8D:
    return U'“'; // left double quote
  case 0x8E:
    return U'”'; // right double quote
  case 0x8F:
    return U'‘'; // left single quote
  case 0x90:
    return U'’'; // right single quote
  case 0x91:
    return U'‚'; // single low-9 quote
  case 0x92:
    return U'™'; // trademark
  case 0x93:
    return U'ﬁ'; // fi ligature
  case 0x94:
    return U'ﬂ'; // fl ligature
  case 0x95:
    return U'Ł'; // L with stroke
  case 0x96:
    return U'Œ'; // OE
  case 0x97:
    return U'Š'; // S with caron
  case 0x98:
    return U'Ÿ'; // Y with diaeresis
  case 0x99:
    return U'Ž'; // Z with caron
  case 0x9A:
    return U'ı'; // dotless i
  case 0x9B:
    return U'ł'; // l with stroke
  case 0x9C:
    return U'œ'; // oe
  case 0x9D:
    return U'š'; // s with caron
  case 0x9E:
    return U'ž'; // z with caron
  case 0xA0:
    return U'€'; // euro
  default:
    return byte;
  }
}

std::string pdf::decode_text_string(const std::string &string) {
  if (string.size() >= 2 && static_cast<std::uint8_t>(string[0]) == 0xFE &&
      static_cast<std::uint8_t>(string[1]) == 0xFF) {
    std::u16string units;
    units.reserve((string.size() - 2) / 2);
    for (std::size_t i = 2; i + 1 < string.size(); i += 2) {
      units.push_back(
          static_cast<char16_t>((static_cast<std::uint8_t>(string[i]) << 8) |
                                static_cast<std::uint8_t>(string[i + 1])));
    }
    return util::string::u16string_to_string(units);
  }
  std::string result;
  for (const char c : string) {
    util::string::append_c32(
        pdf::pdf_doc_encoding_to_unicode(static_cast<std::uint8_t>(c)), result);
  }
  return result;
}

const std::array<std::string_view, 256> &
pdf::base_encoding_table(const BaseEncoding encoding) {
  switch (encoding) {
  case BaseEncoding::standard:
    return encoding_data::standard_encoding;
  case BaseEncoding::win_ansi:
    return encoding_data::win_ansi_encoding;
  case BaseEncoding::mac_roman:
    return encoding_data::mac_roman_encoding;
  }
  throw std::invalid_argument("Unknown base encoding");
}

std::optional<pdf::BaseEncoding>
pdf::base_encoding_from_name(const std::string_view name) {
  if (name == "StandardEncoding") {
    return BaseEncoding::standard;
  }
  if (name == "WinAnsiEncoding") {
    return BaseEncoding::win_ansi;
  }
  if (name == "MacRomanEncoding") {
    return BaseEncoding::mac_roman;
  }
  return std::nullopt;
}

namespace {

/// Parse the algorithmic glyph-name forms: `uniXXXX` (one or more UTF-16 code
/// units, 4 hex digits each) and `uXXXXXX` (a single 4-6 hex-digit scalar). See
/// the AGL specification, "Step 2". Returns empty on any deviation.
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
      if (!unit.has_value()) {
        return {};
      }
      result.push_back(static_cast<char16_t>(*unit));
    }
    return result;
  }

  if (name.starts_with("u") && name.size() >= 5 && name.size() <= 7) {
    const auto scalar = parse_hex(name.substr(1));
    if (!scalar.has_value() || *scalar > 0x10FFFF ||
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

std::u16string pdf::glyph_name_to_unicode(std::string_view glyph_name) {
  // A composite name (`a.alt`, `f_i`) is not yet handled; use the part before
  // the first '.' as the AGL spec prescribes, leave '_' joins to a later
  // refinement.
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

} // namespace odr::internal

namespace odr::internal::pdf {

Encoding::Encoding(const BaseEncoding base)
    : m_base{&base_encoding_table(base)} {}

void Encoding::set_difference(const std::uint8_t code, std::string glyph_name) {
  m_differences[code] = std::move(glyph_name);
}

std::string_view Encoding::glyph_name(const std::uint8_t code) const {
  if (const auto it = m_differences.find(code); it != m_differences.end()) {
    return it->second;
  }
  return m_base->at(code);
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
