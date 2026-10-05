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

/// Map a glyph to the BMP Private Use Area, spilling into PUA-A after 6400.
[[nodiscard]] char32_t pua_code_point(std::uint16_t glyph) noexcept;

/// Map every glyph to its PUA code point, plus valid-glyph entries from @p
/// extra. Extra keys must be BMP code points outside U+E000..U+F8FF. An entry
/// past `numGlyphs` is dropped, because OTS then rejects the whole font.
[[nodiscard]] std::map<char32_t, std::uint16_t>
pua_cmap(std::uint16_t glyph_count,
         const std::map<char32_t, std::uint16_t> &extra = {});

/// Serialize unordered tables with their checksums and head.checkSumAdjustment.
[[nodiscard]] std::string
build_sfnt(std::uint32_t sfnt_version,
           std::vector<std::pair<std::string, std::string>> tables);

/// Serialize a Unicode-to-glyph map using format 4, or format 12 when the
/// code points or encoded length exceed format 4's limits.
[[nodiscard]] std::string
serialize_cmap(const std::map<char32_t, std::uint16_t> &map);

// PDF subsets may omit these tables, which browsers require.

/// nameIDs 1/2/4/6 (family / subfamily / full / PostScript), Windows (3,1),
/// UTF-16BE. An empty @p font_name falls back to "ODR Font".
[[nodiscard]] std::string serialize_name(const std::string &font_name);

/// Version-3.0 `post`: the header alone, i.e. "no glyph names".
[[nodiscard]] std::string serialize_post();

/// Synthesize OS/2 metrics from the em size, bounding box and cmap bounds.
[[nodiscard]] std::string serialize_os2(std::uint16_t units_per_em,
                                        std::int16_t y_min, std::int16_t y_max,
                                        std::uint16_t first_char,
                                        std::uint16_t last_char);

/// Replace the cmap with pua_cmap(glyph_count, extra) for browser rendering.
void reencode_to_pua(sfnt::SfntFont &font,
                     const std::map<char32_t, std::uint16_t> &extra = {});

} // namespace odr::internal::font
