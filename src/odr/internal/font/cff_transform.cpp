#include <odr/internal/font/cff_transform.hpp>

#include <odr/internal/font/cff_font.hpp>
#include <odr/internal/font/sfnt_transform.hpp>
#include <odr/internal/util/byte_string.hpp>

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace odr::internal::font::cff {

namespace {

namespace bs = util::byte_string;

/// A `head` table (54 bytes). `checkSumAdjustment` (offset 8) is left zero —
/// `build_sfnt` patches it after laying out the file.
std::string serialize_head(const std::uint16_t units_per_em,
                           const FontBBox bbox) {
  std::string head;
  bs::put_u32_be(head, 0x00010000); // version 1.0
  bs::put_u32_be(head, 0);          // fontRevision
  bs::put_u32_be(head, 0); // checkSumAdjustment (patched by build_sfnt)
  bs::put_u32_be(head, 0x5f0f3cf5); // magicNumber
  bs::put_u16_be(head, 0);          // flags
  bs::put_u16_be(head, units_per_em);
  bs::put_u32_be(head, 0); // created (hi/lo)
  bs::put_u32_be(head, 0);
  bs::put_u32_be(head, 0); // modified (hi/lo)
  bs::put_u32_be(head, 0);
  bs::put_u16_be(head, static_cast<std::uint16_t>(bbox.x_min));
  bs::put_u16_be(head, static_cast<std::uint16_t>(bbox.y_min));
  bs::put_u16_be(head, static_cast<std::uint16_t>(bbox.x_max));
  bs::put_u16_be(head, static_cast<std::uint16_t>(bbox.y_max));
  bs::put_u16_be(head, 0); // macStyle
  bs::put_u16_be(head, 8); // lowestRecPPEM
  bs::put_u16_be(head, 2); // fontDirectionHint
  bs::put_u16_be(head, 0); // indexToLocFormat
  bs::put_u16_be(head, 0); // glyphDataFormat
  return head;
}

/// An `hhea` table (36 bytes).
std::string serialize_hhea(const FontBBox bbox,
                           const std::uint16_t advance_width_max,
                           const std::uint16_t num_h_metrics) {
  std::string hhea;
  bs::put_u32_be(hhea, 0x00010000);                             // version 1.0
  bs::put_u16_be(hhea, static_cast<std::uint16_t>(bbox.y_max)); // ascender
  bs::put_u16_be(hhea, static_cast<std::uint16_t>(bbox.y_min)); // descender
  bs::put_u16_be(hhea, 0);                                      // lineGap
  bs::put_u16_be(hhea, advance_width_max);
  bs::put_u16_be(hhea,
                 static_cast<std::uint16_t>(bbox.x_min)); // minLeftSideBearing
  bs::put_u16_be(hhea, 0);                                // minRightSideBearing
  bs::put_u16_be(hhea, static_cast<std::uint16_t>(bbox.x_max)); // xMaxExtent
  bs::put_u16_be(hhea, 1); // caretSlopeRise
  bs::put_u16_be(hhea, 0); // caretSlopeRun
  bs::put_u16_be(hhea, 0); // caretOffset
  bs::put_u16_be(hhea, 0); // reserved
  bs::put_u16_be(hhea, 0);
  bs::put_u16_be(hhea, 0);
  bs::put_u16_be(hhea, 0);
  bs::put_u16_be(hhea, 0); // metricDataFormat
  bs::put_u16_be(hhea, num_h_metrics);
  return hhea;
}

/// A `maxp` table for a CFF font (version 0.5, just numGlyphs).
std::string serialize_maxp(const std::uint16_t glyph_count) {
  std::string maxp;
  bs::put_u32_be(maxp, 0x00005000); // version 0.5
  bs::put_u16_be(maxp, glyph_count);
  return maxp;
}

/// An `hmtx` table: one (advanceWidth, lsb) pair per glyph; lsb is left 0
/// (display takes the advance from here, the outline carries its own bearing).
std::string serialize_hmtx(const CffFont &font) {
  std::string hmtx;
  for (std::uint16_t glyph = 0; glyph < font.glyph_count(); ++glyph) {
    bs::put_u16_be(hmtx, font.advance_width(glyph));
    bs::put_u16_be(hmtx, 0); // leftSideBearing
  }
  return hmtx;
}

} // namespace

} // namespace odr::internal::font::cff

namespace odr::internal::font {

std::string cff::wrap_to_otf(const CffFont &font,
                             const std::map<char32_t, std::uint16_t> &extra) {
  const std::uint16_t glyphs = font.glyph_count();

  // Glyphs past the 6400-slot BMP PUA overflow into Supplementary PUA-A, which
  // serialize_cmap covers with a format-12 subtable.
  const std::map<char32_t, std::uint16_t> pua = pua_cmap(glyphs, extra);

  std::uint16_t advance_width_max = 0;
  for (std::uint16_t glyph = 0; glyph < glyphs; ++glyph) {
    advance_width_max = std::max(advance_width_max, font.advance_width(glyph));
  }

  const FontBBox bbox = font.bounding_box();
  // `cmap`-bounding OS/2 indices over the full map (the real-Unicode entries
  // can sit below the PUA range).
  const char32_t first = pua.empty() ? 0 : pua.begin()->first;
  const char32_t last = pua.empty() ? 0 : pua.rbegin()->first;

  std::vector<std::pair<std::string, std::string>> tables;
  tables.emplace_back("CFF ", std::string(font.data()));
  tables.emplace_back("head", serialize_head(font.units_per_em(), bbox));
  tables.emplace_back("hhea", serialize_hhea(bbox, advance_width_max, glyphs));
  tables.emplace_back("maxp", serialize_maxp(glyphs));
  tables.emplace_back("hmtx", serialize_hmtx(font));
  tables.emplace_back("cmap", serialize_cmap(pua));
  tables.emplace_back("name", serialize_name(font.name()));
  tables.emplace_back("post", serialize_post());
  // OS/2 usFirst/usLastCharIndex are u16; a beyond-BMP PUA code point (large
  // glyph counts overflow into Supplementary PUA-A) is clamped to 0xFFFF.
  tables.emplace_back(
      "OS/2",
      serialize_os2(
          font.units_per_em(), bbox.y_min, bbox.y_max,
          static_cast<std::uint16_t>(std::min<char32_t>(first, 0xffff)),
          static_cast<std::uint16_t>(std::min<char32_t>(last, 0xffff))));

  return build_sfnt(0x4f54544f /* 'OTTO' */, std::move(tables));
}

} // namespace odr::internal::font
