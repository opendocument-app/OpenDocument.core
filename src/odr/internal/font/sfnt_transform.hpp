#pragma once

#include <cstdint>
#include <iosfwd>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace odr::internal::font {

namespace sfnt {
class SfntFont;
}

/// The deterministic Private Use Area code point that the uniform
/// re-encode assigns to glyph @p glyph:
/// `U+E000 + glyph` in the BMP PUA. Every consumer (the specimen page, the PDF
/// `@font-face` emission) derives the displayed code point from this — no
/// per-font table needed.
[[nodiscard]] char32_t pua_code_point(std::uint16_t glyph) noexcept;

/// Serialize an SFNT from its tables to @p out, computing the table directory,
/// per-table checksums and `head.checkSumAdjustment`. @p tables need not be
/// sorted; a `head` table is patched in place with the final adjustment.
///
/// The whole-file checksum is additive over the 4-byte-aligned table layout, so
/// it equals `checksum(header+directory) + Σ checksum(table)` — computed
/// analytically and the adjustment patched into `head` before the single
/// forward write, so @p out need only be a forward sink (no seek, no tee).
void build_sfnt(std::ostream &out, std::uint32_t sfnt_version,
                std::vector<std::pair<std::string, std::string>> tables);

/// Serialize a code point -> glyph map into a `cmap` table.
///
/// LIMITATION: emits a single Windows (3,1) format-4 subtable, so only BMP code
/// points (<= U+FFFF) are supported; a map containing a code point beyond the
/// BMP (which would require a format-12 subtable) throws `std::runtime_error`.
/// Within the BMP there is no further restriction: the map is split into
/// maximal arithmetic runs (consecutive code points mapping to consecutive
/// glyphs, `idRangeOffset = 0`), and a run of length one is trivially
/// arithmetic, so the `glyphIdArray` path is never needed. This covers the
/// uniform PUA re-encode (one run) and ordinary remaps; format-12 / multi-plane
/// coverage is a follow-up.
[[nodiscard]] std::string
serialize_cmap(const std::map<char32_t, std::uint16_t> &map);

/// Re-encode @p font in place for the browser: replace its `cmap` with a fresh
/// map from the deterministic PUA code points (`pua_code_point`) to *every*
/// glyph, so the font renders every glyph — including ones the original `cmap`
/// never reached — when loaded via `@font-face`. `font.write()` then emits the
/// re-encoded SFNT.
///
/// Throws `std::runtime_error` if the glyph count exceeds the BMP PUA capacity
/// (6400); multi-plane PUA spill-over is a follow-up.
void reencode_to_pua(sfnt::SfntFont &font);

} // namespace odr::internal::font
