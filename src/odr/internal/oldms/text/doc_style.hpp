#pragma once

#include <odr/style.hpp>

#include <odr/internal/oldms/text/doc_structs.hpp>

#include <cstdint>
#include <iosfwd>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace odr::internal::oldms::text {

/// Owns the document's resolved character styles — indexed by the style index
/// stored on paragraph/span elements, 0 being the default style — and the
/// font names `TextStyle::font_name` points into.
class StyleRegistry final {
public:
  /// Stores the font table (SttbfFfn names); must be set before any style
  /// referencing a font is added.
  void set_font_names(std::vector<std::string> names);
  [[nodiscard]] std::span<const std::string> font_names() const;

  std::uint32_t add_style(TextStyle style);
  /// Throws if the index has no style.
  [[nodiscard]] const TextStyle &text_style(std::uint32_t index) const;

private:
  /// Owns the font names: `TextStyle::font_name` (`const char *`) points into
  /// them. Never modified after `set_font_names` (moving the registry is fine
  /// — the strings themselves do not move).
  std::vector<std::string> m_font_names;
  std::vector<TextStyle> m_styles;
};

/// Applies the character SPRMs of a Chpx grpprl ([MS-DOC] 2.6.1) on top of
/// `style`; non-character SPRMs are skipped via their operand size.
/// `font_names` resolves sprmCRgFtc0 (the strings must outlive the style —
/// `TextStyle::font_name` points into them).
TextStyle apply_character_sprms(TextStyle style, std::string_view grpprl,
                                std::span<const std::string> font_names);

/// Reads the font names (FFN.xszFfn) of the SttbfFfn ([MS-DOC] 2.9.286) at
/// `fc` in the table stream; empty for lcb == 0.
std::vector<std::string> read_font_names(std::istream &table_stream,
                                         FcLcb sttbf_ffn);

} // namespace odr::internal::oldms::text
