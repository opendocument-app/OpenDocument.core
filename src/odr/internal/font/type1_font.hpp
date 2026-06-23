#pragma once

#include <odr/font.hpp>

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace odr::internal::font::type1 {

/// One glyph of a Type1 font: its PostScript name and its **decrypted** Type1
/// charstring (charstring encryption removed, `/lenIV` leading bytes dropped).
struct Glyph {
  std::string name;
  std::string charstring;
};

/// @brief Parses an Adobe Type1 font program into its decrypted parts.
///
/// A Type1 program has three sections: a clear-text header (font dictionary up
/// to `eexec`), an `eexec`-encrypted private portion (`/Subrs`,
/// `/CharStrings`), and a zero-padded trailer. This reads the header for
/// `/FontMatrix`,
/// `/FontBBox`, `/Encoding` and `/FontName`, decrypts the `eexec` section
/// (`type1_crypt`) and extracts every glyph's decrypted charstring plus the
/// `/Subrs`. It does **not** yet interpret the charstrings — that is the
/// Type1 -> Type2 translation that follows, feeding 3.4's CFF -> OTF path.
///
/// Throws `std::runtime_error` when the program has no `eexec` section or no
/// `/CharStrings`.
class Type1Program {
public:
  /// Cheap magic test: the PostScript font sentinel (`%!PS-AdobeFont`,
  /// `%!FontType1`) or a PFB segment marker (`0x80`).
  [[nodiscard]] static bool is_type1(std::string_view data);

  /// Parse @p program (the raw `/FontFile` bytes, PFB markers stripped if
  /// present).
  explicit Type1Program(std::string_view program);

  [[nodiscard]] std::string_view name() const noexcept { return m_name; }
  /// The 6-element `/FontMatrix` (defaults to `[0.001 0 0 0.001 0 0]`).
  [[nodiscard]] const std::vector<double> &font_matrix() const noexcept {
    return m_font_matrix;
  }
  [[nodiscard]] FontBBox font_bbox() const noexcept { return m_font_bbox; }

  /// `/Encoding` as code -> glyph name. Empty when the font uses
  /// `StandardEncoding` (see `standard_encoding`).
  [[nodiscard]] const std::map<int, std::string> &encoding() const noexcept {
    return m_encoding;
  }
  [[nodiscard]] bool standard_encoding() const noexcept {
    return m_standard_encoding;
  }

  /// Decrypted glyphs in declaration order.
  [[nodiscard]] const std::vector<Glyph> &glyphs() const noexcept {
    return m_glyphs;
  }
  /// Decrypted `/Subrs`, indexed by subr number.
  [[nodiscard]] const std::vector<std::string> &subrs() const noexcept {
    return m_subrs;
  }

private:
  void parse_clear(std::string_view clear);
  void parse_private(std::string_view decrypted);

  std::string m_name;
  std::vector<double> m_font_matrix{0.001, 0.0, 0.0, 0.001, 0.0, 0.0};
  FontBBox m_font_bbox{};
  std::map<int, std::string> m_encoding;
  bool m_standard_encoding{true};
  std::vector<Glyph> m_glyphs;
  std::vector<std::string> m_subrs;
  int m_len_iv{4};
};

/// Convert a parsed Type1 program to a **CFF** font: translate every glyph's
/// charstring to Type2 (`to_type2`, flattening the program's `/Subrs`) and
/// assemble via the CFF builder, with `.notdef` placed at glyph 0. The result
/// is a bare CFF that `cff::CffFont` reads and `cff::wrap_to_otf` wraps for the
/// browser — so an embedded Type1 font reuses the entire 3.4 CFF path.
[[nodiscard]] std::string to_cff(const Type1Program &program);

} // namespace odr::internal::font::type1
