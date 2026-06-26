#pragma once

#include <cstdint>
#include <map>
#include <string>

namespace odr::internal::font::cff {

class CffFont;

/// Wrap a bare CFF font into a browser-loadable OpenType-CFF (`OTTO`) SFNT.
///
/// A bare CFF carries none of the SFNT metric tables a browser (and the OTS
/// sanitizer) require, so this synthesizes the skeleton — `head` / `hhea` /
/// `maxp` (v0.5) / `hmtx` / `name` / `post` / `OS/2` — from the
/// `abstract::Font` facts and embeds the original CFF verbatim as the `CFF `
/// table (pass-through, no outline interpretation). The `cmap` is the **uniform
/// PUA re-encode**: `pua_code_point(glyph) -> glyph` over every
/// glyph, so the font renders every glyph — including charset-unreachable ones
/// — when loaded via `@font-face`, matching the PUA code points the PDF HTML
/// layer emits.
///
/// @p extra adds real-Unicode -> glyph entries alongside the PUA range, so a
/// run whose codes map 1:1 to those scalars can render the *real* Unicode
/// directly (the HTML layer then collapses its dual selectable/visible spans
/// into one). Keys must be in the BMP and outside the PUA (`U+E000..U+F8FF`);
/// the caller guarantees this. The PUA range is always kept as a fallback.
///
/// Reuses the `sfnt_transform` serializers (`build_sfnt`, `serialize_cmap`,
/// `serialize_post`, `serialize_os2`). Throws `std::runtime_error` if the glyph
/// count exceeds the BMP PUA capacity (6400).
[[nodiscard]] std::string
wrap_to_otf(const CffFont &font,
            const std::map<char32_t, std::uint16_t> &extra = {});

} // namespace odr::internal::font::cff
