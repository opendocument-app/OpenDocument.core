#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace odr::internal::pdf {

/// The 14 standard fonts (ISO 32000-1 9.6.2.2) whose glyph metrics ship in the
/// Core AFM tables (`pdf_afm_data`). The order matches `afm_data::fonts`.
enum class StandardFont : std::uint8_t {
  helvetica,
  helvetica_bold,
  helvetica_oblique,
  helvetica_bold_oblique,
  times_roman,
  times_bold,
  times_italic,
  times_bold_italic,
  courier,
  courier_bold,
  courier_oblique,
  courier_bold_oblique,
  symbol,
  zapf_dingbats,
};

/// How a non-embedded font is rendered: with the browser's own fonts, chosen by
/// a CSS `font-family` stack, and placed with the standard-14 AFM advance
/// widths (`metrics`) when the font maps onto one of them. Resolved once at
/// parse time (`resolve_font_substitute`) and carried on `Font::substitute`.
struct FontSubstitute {
  /// A generic CSS `font-family` value, e.g. `"Helvetica,Arial,sans-serif"`.
  /// Generic families always resolve, so a viewer never sees a missing glyph
  /// box; the shapes are approximate, the metrics (below) make placement exact.
  std::string css_family;
  bool bold{false};
  bool italic{false};
  /// The standard-14 font whose AFM widths drive `advance_width`, or `nullopt`
  /// when the font resembles none of them (then `/Widths`/`/MissingWidth` are
  /// the only metric source, as before).
  std::optional<StandardFont> metrics;
};

/// Choose a substitute for a non-embedded simple font from its `/BaseFont` name
/// and `/FontDescriptor` hints (`/Flags`, `/FontWeight`, `/ItalicAngle`;
/// `font_weight <= 0` and `italic_angle == 0` mean "absent"). A subset prefix
/// (`ABCDEF+`) on the name is ignored. Never fails: an unrecognized font falls
/// back to a sans-serif stack with Helvetica metrics.
[[nodiscard]] FontSubstitute resolve_font_substitute(std::string_view base_font,
                                                     std::uint32_t flags,
                                                     std::int32_t font_weight,
                                                     double italic_angle);

/// The advance width of a glyph (by name) in `font`, glyph space (1/1000 em),
/// or `nullopt` when the font has no such glyph.
[[nodiscard]] std::optional<double> afm_width(StandardFont font,
                                              std::string_view glyph_name);

/// The advance width of a code in `font`'s built-in encoding, glyph space
/// (1/1000 em), or `nullopt` when the slot is empty. The fallback when a font
/// carries no `/Encoding` (notably Symbol/ZapfDingbats).
[[nodiscard]] std::optional<double> afm_code_width(StandardFont font,
                                                   std::uint8_t code);

/// The font's `Ascender` metric, em units (glyph space / 1000); used as the
/// baseline-shift fallback for a substitute with no `/FontDescriptor`
/// `/Ascent`.
[[nodiscard]] double afm_ascender(StandardFont font);

} // namespace odr::internal::pdf
