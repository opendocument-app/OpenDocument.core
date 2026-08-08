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
/// table (pass-through, no outline interpretation).
///
/// The `cmap` is `pua_cmap(glyph_count, extra)`, so the font renders every
/// glyph — including charset-unreachable ones — at the PUA code points the PDF
/// HTML layer emits.
[[nodiscard]] std::string
wrap_to_otf(const CffFont &font,
            const std::map<char32_t, std::uint16_t> &extra = {});

} // namespace odr::internal::font::cff
