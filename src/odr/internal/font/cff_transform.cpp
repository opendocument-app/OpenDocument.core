#include <odr/internal/font/cff_transform.hpp>

#include <odr/internal/font/cff_font.hpp>
#include <odr/internal/font/sfnt_transform.hpp>

#include <algorithm>
#include <cstdint>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace odr::internal::font::cff {

namespace {

void put16(std::string &s, const std::uint16_t v) {
  s += static_cast<char>(v >> 8);
  s += static_cast<char>(v & 0xff);
}

void put32(std::string &s, const std::uint32_t v) {
  put16(s, static_cast<std::uint16_t>(v >> 16));
  put16(s, static_cast<std::uint16_t>(v & 0xffff));
}

/// A `head` table (54 bytes). `checkSumAdjustment` (offset 8) is left zero —
/// `build_sfnt` patches it after laying out the file.
std::string serialize_head(const std::uint16_t units_per_em,
                           const FontBBox bbox) {
  std::string head;
  put32(head, 0x00010000); // version 1.0
  put32(head, 0);          // fontRevision
  put32(head, 0);          // checkSumAdjustment (patched by build_sfnt)
  put32(head, 0x5f0f3cf5); // magicNumber
  put16(head, 0);          // flags
  put16(head, units_per_em);
  put32(head, 0); // created (hi/lo)
  put32(head, 0);
  put32(head, 0); // modified (hi/lo)
  put32(head, 0);
  put16(head, static_cast<std::uint16_t>(bbox.x_min));
  put16(head, static_cast<std::uint16_t>(bbox.y_min));
  put16(head, static_cast<std::uint16_t>(bbox.x_max));
  put16(head, static_cast<std::uint16_t>(bbox.y_max));
  put16(head, 0); // macStyle
  put16(head, 8); // lowestRecPPEM
  put16(head, 2); // fontDirectionHint
  put16(head, 0); // indexToLocFormat
  put16(head, 0); // glyphDataFormat
  return head;
}

/// An `hhea` table (36 bytes).
std::string serialize_hhea(const FontBBox bbox,
                           const std::uint16_t advance_width_max,
                           const std::uint16_t num_h_metrics) {
  std::string hhea;
  put32(hhea, 0x00010000);                             // version 1.0
  put16(hhea, static_cast<std::uint16_t>(bbox.y_max)); // ascender
  put16(hhea, static_cast<std::uint16_t>(bbox.y_min)); // descender
  put16(hhea, 0);                                      // lineGap
  put16(hhea, advance_width_max);
  put16(hhea, static_cast<std::uint16_t>(bbox.x_min)); // minLeftSideBearing
  put16(hhea, 0);                                      // minRightSideBearing
  put16(hhea, static_cast<std::uint16_t>(bbox.x_max)); // xMaxExtent
  put16(hhea, 1);                                      // caretSlopeRise
  put16(hhea, 0);                                      // caretSlopeRun
  put16(hhea, 0);                                      // caretOffset
  put16(hhea, 0);                                      // reserved
  put16(hhea, 0);
  put16(hhea, 0);
  put16(hhea, 0);
  put16(hhea, 0); // metricDataFormat
  put16(hhea, num_h_metrics);
  return hhea;
}

/// A `maxp` table for a CFF font (version 0.5, just numGlyphs).
std::string serialize_maxp(const std::uint16_t glyph_count) {
  std::string maxp;
  put32(maxp, 0x00005000); // version 0.5
  put16(maxp, glyph_count);
  return maxp;
}

/// An `hmtx` table: one (advanceWidth, lsb) pair per glyph; lsb is left 0
/// (display takes the advance from here, the outline carries its own bearing).
std::string serialize_hmtx(const CffFont &font) {
  std::string hmtx;
  for (std::uint16_t glyph = 0; glyph < font.glyph_count(); ++glyph) {
    put16(hmtx, font.advance_width(glyph));
    put16(hmtx, 0); // leftSideBearing
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
      put16(out, static_cast<std::uint8_t>(c));
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
  put16(table, 0);     // format 0
  put16(table, count); // count
  put16(table, storage_offset);

  std::string storage;
  for (const Record &record : records) {
    put16(table, 3);     // platformID: Windows
    put16(table, 1);     // encodingID: Unicode BMP
    put16(table, 0x409); // languageID: en-US
    put16(table, record.name_id);
    put16(table, static_cast<std::uint16_t>(record.value.size()));
    put16(table, static_cast<std::uint16_t>(storage.size()));
    storage += record.value;
  }
  table += storage;
  return table;
}

} // namespace

std::string wrap_to_otf(const CffFont &font) {
  const std::uint16_t glyphs = font.glyph_count();

  // The uniform PUA re-encode (stage 3.1): pua_code_point(glyph) -> glyph over
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

} // namespace odr::internal::font::cff
