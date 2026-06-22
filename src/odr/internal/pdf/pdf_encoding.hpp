#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace odr::internal::pdf {

/// The predefined simple-font base encodings (ISO 32000-1 Annex D). Each maps a
/// 1-byte character code to an Adobe glyph name.
enum class BaseEncoding {
  standard,  ///< StandardEncoding
  win_ansi,  ///< WinAnsiEncoding
  mac_roman, ///< MacRomanEncoding
};

/// The 256-entry glyph-name table for `encoding`; the empty string is
/// `.notdef`. Entries are `std::string_view`s into static data.
[[nodiscard]] const std::array<std::string_view, 256> &
base_encoding_table(BaseEncoding encoding);

/// Maps a `/BaseEncoding` (or `/Encoding`) name to a `BaseEncoding`; `nullopt`
/// for names this stage does not yet support (e.g. `MacExpertEncoding`).
[[nodiscard]] std::optional<BaseEncoding>
base_encoding_from_name(std::string_view name);

/// Glyph name -> Unicode (UTF-16) via the Adobe Glyph List, plus the
/// algorithmic `uniXXXX` / `uXXXXXX` forms (ISO 32000-1 9.10.2 / the AGL
/// specification). Returns an empty string for a name with no mapping — the
/// caller treats that as "no Unicode".
[[nodiscard]] std::u16string glyph_name_to_unicode(std::string_view glyph_name);

/// A simple font's `/Encoding`: a base encoding optionally overlaid with
/// `/Differences`, resolving each 1-byte code to a glyph name and then to
/// Unicode. The text-extraction fallback for simple fonts that carry no
/// `ToUnicode` CMap.
class Encoding {
public:
  explicit Encoding(BaseEncoding base);

  /// Override a single code's glyph name (a `/Differences` entry).
  void set_difference(std::uint8_t code, std::string glyph_name);

  /// The glyph name for `code` (a `/Differences` override wins over the base);
  /// empty for `.notdef`.
  [[nodiscard]] std::string_view glyph_name(std::uint8_t code) const;

  /// Translate a string of 1-byte codes to Unicode by mapping each code to its
  /// glyph name and then through the AGL.
  [[nodiscard]] std::string translate_string(const std::string &codes) const;

private:
  const std::array<std::string_view, 256> *m_base{};
  std::unordered_map<std::uint8_t, std::string> m_differences;
};

} // namespace odr::internal::pdf
