#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace odr::internal::font {

namespace sfnt {
class SfntFont;
}

/// @brief The deterministic Private Use Area code point that the uniform
/// re-encode (stage 3, decision 2026-06-19) assigns to glyph @p glyph:
/// `U+E000 + glyph` in the BMP PUA. Every consumer (the specimen page, the PDF
/// `@font-face` emission) derives the displayed code point from this — no
/// per-font table needed.
[[nodiscard]] char32_t pua_code_point(std::uint16_t glyph) noexcept;

/// @brief Serialize an SFNT from its tables, computing the table directory,
/// per-table checksums and `head.checkSumAdjustment`. @p tables need not be
/// sorted; a `head` table is patched in place with the final adjustment.
[[nodiscard]] std::string
build_sfnt(std::uint32_t sfnt_version,
           std::vector<std::pair<std::string, std::string>> tables);

/// @brief Re-encode an SFNT font for the browser: copy every table through
/// unchanged except `cmap`, which is replaced by a fresh map from the
/// deterministic PUA code points (`pua_code_point`) to *every* glyph, then
/// rebuild the directory and checksums. The result is a valid SFNT that loads
/// via `@font-face` and renders every glyph — including ones the original
/// `cmap` never reached.
///
/// Throws `std::runtime_error` if the glyph count exceeds the BMP PUA capacity
/// (6400); multi-plane PUA spill-over is a follow-up.
[[nodiscard]] std::string reencode_to_pua(const sfnt::SfntFont &font);

} // namespace odr::internal::font
