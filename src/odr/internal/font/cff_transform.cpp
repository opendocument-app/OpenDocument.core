#include <odr/internal/font/cff_transform.hpp>

#include <odr/internal/font/cff_font.hpp>
#include <odr/internal/font/sfnt_transform.hpp>
#include <odr/internal/util/byte_string.hpp>

#include <algorithm>
#include <cstdint>
#include <map>
#include <sstream>
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

/// A minimal `name` table: nameIDs 1/2/4/6 (family / subfamily / full /
/// PostScript), Windows platform (3,1), UTF-16BE. OTS requires a `name` table,
/// and a bare CFF has none.
std::string serialize_name(const std::string &font_name) {
  const std::string family = font_name.empty() ? "ODR Font" : font_name;
  const auto utf16be = [](const std::string &ascii) {
    std::string out;
    for (const char c : ascii) {
      bs::put_u16_be(out, static_cast<std::uint8_t>(c));
    }
    return out;
  };
  struct Record {
    std::uint16_t name_id;
    std::string value;
  };
  const std::vector<Record> records = {
      {1, utf16be(family)},
      {2, utf16be("Regular")},
      {4, utf16be(family)},
      {6, utf16be(family)},
  };

  const auto count = static_cast<std::uint16_t>(records.size());
  const std::uint16_t storage_offset = 6 + count * 12;

  std::string table;
  bs::put_u16_be(table, 0);     // format 0
  bs::put_u16_be(table, count); // count
  bs::put_u16_be(table, storage_offset);

  std::string storage;
  for (const Record &record : records) {
    bs::put_u16_be(table, 3);     // platformID: Windows
    bs::put_u16_be(table, 1);     // encodingID: Unicode BMP
    bs::put_u16_be(table, 0x409); // languageID: en-US
    bs::put_u16_be(table, record.name_id);
    bs::put_u16_be(table, static_cast<std::uint16_t>(record.value.size()));
    bs::put_u16_be(table, static_cast<std::uint16_t>(storage.size()));
    storage += record.value;
  }
  table += storage;
  return table;
}

} // namespace

} // namespace odr::internal::font::cff

namespace odr::internal::font {

std::string cff::wrap_to_otf(const CffFont &font) {
  const std::uint16_t glyphs = font.glyph_count();

  // The uniform PUA re-encode: pua_code_point(glyph) -> glyph over
  // every glyph. serialize_cmap throws if a code point is beyond the BMP, which
  // also bounds the glyph count to the PUA capacity.
  std::map<char32_t, std::uint16_t> pua;
  for (std::uint16_t glyph = 0; glyph < glyphs; ++glyph) {
    pua[pua_code_point(glyph)] = glyph;
  }

  std::uint16_t advance_width_max = 0;
  for (std::uint16_t glyph = 0; glyph < glyphs; ++glyph) {
    advance_width_max = std::max(advance_width_max, font.advance_width(glyph));
  }

  const FontBBox bbox = font.bounding_box();
  const char32_t first = pua_code_point(0);
  const char32_t last = pua_code_point(glyphs == 0 ? 0 : glyphs - 1);

  std::vector<std::pair<std::string, std::string>> tables;
  tables.emplace_back("CFF ", std::string(font.data()));
  tables.emplace_back("head", serialize_head(font.units_per_em(), bbox));
  tables.emplace_back("hhea", serialize_hhea(bbox, advance_width_max, glyphs));
  tables.emplace_back("maxp", serialize_maxp(glyphs));
  tables.emplace_back("hmtx", serialize_hmtx(font));
  tables.emplace_back("cmap", serialize_cmap(pua));
  tables.emplace_back("name", serialize_name(font.name()));
  tables.emplace_back("post", serialize_post());
  tables.emplace_back("OS/2",
                      serialize_os2(font.units_per_em(), bbox.y_min, bbox.y_max,
                                    static_cast<std::uint16_t>(first),
                                    static_cast<std::uint16_t>(last)));

  std::ostringstream out;
  build_sfnt(out, 0x4f54544f /* 'OTTO' */, std::move(tables));
  return std::move(out).str();
}

} // namespace odr::internal::font
