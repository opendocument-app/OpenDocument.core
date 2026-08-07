#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace odr::internal::font {

namespace sfnt {
class SfntFont;
}

/// The deterministic Private Use Area code point the uniform re-encode assigns
/// to glyph @p glyph: `U+E000 + glyph` while the BMP PUA lasts (6400 slots),
/// then Supplementary PUA-A from `U+F0000`. Every consumer derives the
/// displayed code point from this — no per-font table needed.
[[nodiscard]] char32_t pua_code_point(std::uint16_t glyph) noexcept;

/// The uniform PUA re-encode as a `cmap` model: `pua_code_point(glyph) ->
/// glyph` for every glyph below @p glyph_count, plus @p extra's real-Unicode
/// entries alongside it (keys must be in the BMP and outside `U+E000..U+F8FF`
/// so they never shadow a glyph's own PUA code point; the caller guarantees
/// this). An @p extra entry naming a glyph the font does not have is dropped —
/// a single `cmap` reference past `numGlyphs` makes the OTS sanitizer reject
/// the *entire* font, so every glyph would render as tofu.
[[nodiscard]] std::map<char32_t, std::uint16_t>
pua_cmap(std::uint16_t glyph_count,
         const std::map<char32_t, std::uint16_t> &extra = {});

/// Serialize an SFNT from its tables, computing the table directory, per-table
/// checksums and `head.checkSumAdjustment`. @p tables need not be sorted; a
/// `head` table is patched in place with the final adjustment.
[[nodiscard]] std::string
build_sfnt(std::uint32_t sfnt_version,
           std::vector<std::pair<std::string, std::string>> tables);

/// Serialize a code point -> glyph map into a `cmap` table: one Windows
/// subtable, format 4 (3,1) while the map stays in the BMP, format 12 (3,10)
/// once it reaches beyond it. Both split the map into maximal runs of
/// consecutive code points mapping to consecutive glyphs, so format 4 never
/// needs a `glyphIdArray`.
[[nodiscard]] std::string
serialize_cmap(const std::map<char32_t, std::uint16_t> &map);

// OTS (the font sanitizer in Chrome/Firefox) rejects a font that is missing
// `name`, `post` or `OS/2` — the browser then drops the `@font-face` and
// renders tofu — and PDF-embedded fonts routinely omit all three. The three
// below synthesize minimal stand-ins; OTS reconciles the fields they leave
// neutral (e.g. `fsSelection` against `head`).

/// nameIDs 1/2/4/6 (family / subfamily / full / PostScript), Windows (3,1),
/// UTF-16BE. An empty @p font_name falls back to "ODR Font".
[[nodiscard]] std::string serialize_name(const std::string &font_name);

/// Version-3.0 `post`: the header alone, i.e. "no glyph names".
[[nodiscard]] std::string serialize_post();

/// Version-4 `OS/2`, neutral weight and width. @p units_per_em scales the
/// sub/superscript and strikeout defaults; @p y_min / @p y_max are the font
/// bounding box (ascender/descender fall back to 0.8/0.2 em when degenerate);
/// @p first_char / @p last_char bound the `cmap`.
[[nodiscard]] std::string serialize_os2(std::uint16_t units_per_em,
                                        std::int16_t y_min, std::int16_t y_max,
                                        std::uint16_t first_char,
                                        std::uint16_t last_char);

/// Re-encode @p font in place for the browser: replace its `cmap` with a fresh
/// map from the deterministic PUA code points (`pua_code_point`) to *every*
/// glyph, so the font renders every glyph — including ones the original `cmap`
/// never reached — when loaded via `@font-face`. `font.write()` then emits the
/// re-encoded SFNT.
///
/// @p extra adds real-Unicode -> glyph entries alongside the PUA range (see
/// `pua_cmap`), so a run whose codes map 1:1 to those scalars can render the
/// *real* Unicode directly — the HTML layer then collapses its dual
/// selectable/visible spans into one.
void reencode_to_pua(sfnt::SfntFont &font,
                     const std::map<char32_t, std::uint16_t> &extra = {});

} // namespace odr::internal::font
