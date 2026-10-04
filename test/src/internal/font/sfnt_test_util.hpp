#pragma once

#include <odr/font.hpp>
#include <odr/internal/util/byte_string.hpp>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace odr::test::font {

namespace bs = odr::internal::util::byte_string;

// Raw SFNT fixtures independent of the production serializers.

inline std::string head_table(const odr::FontBBox bbox = {}) {
  std::string table(54, '\0');
  bs::write_u16_be(table, 18, 1000);
  bs::write_u16_be(table, 36, static_cast<std::uint16_t>(bbox.x_min));
  bs::write_u16_be(table, 38, static_cast<std::uint16_t>(bbox.y_min));
  bs::write_u16_be(table, 40, static_cast<std::uint16_t>(bbox.x_max));
  bs::write_u16_be(table, 42, static_cast<std::uint16_t>(bbox.y_max));
  return table;
}

inline std::string maxp_table(const std::uint16_t glyphs) {
  std::string t;
  bs::put_u32_be(t, 0x00010000); // version 1.0
  bs::put_u16_be(t, glyphs);
  t.resize(32, '\0');
  return t;
}

inline std::string hhea_table(const std::uint16_t number_of_h_metrics) {
  std::string t(36, '\0');
  t[34] = static_cast<char>(number_of_h_metrics >> 8);
  t[35] = static_cast<char>(number_of_h_metrics & 0xff);
  return t;
}

inline std::string hmtx_table(const std::vector<std::uint16_t> &advances) {
  std::string t;
  for (const std::uint16_t a : advances) {
    bs::put_u16_be(t, a); // advanceWidth
    bs::put_u16_be(t, 0); // leftSideBearing
  }
  return t;
}

inline std::string cmap_format4(const char16_t start,
                                const std::uint16_t count) {
  std::string t;
  bs::put_u16_be(t, 4);  // format
  bs::put_u16_be(t, 32); // length
  bs::put_u16_be(t, 0);  // language
  bs::put_u16_be(t, 4);  // segCountX2 (2 segments)
  bs::put_u16_be(t, 0);  // searchRange
  bs::put_u16_be(t, 0);  // entrySelector
  bs::put_u16_be(t, 0);  // rangeShift
  bs::put_u16_be(t,
                 static_cast<std::uint16_t>(start + count - 1)); // endCode[0]
  bs::put_u16_be(t, 0xffff);                                     // endCode[1]
  bs::put_u16_be(t, 0);                                          // reservedPad
  bs::put_u16_be(t, start);                                      // startCode[0]
  bs::put_u16_be(t, 0xffff);                                     // startCode[1]
  bs::put_u16_be(
      t, static_cast<std::uint16_t>(1 - start)); // idDelta[0] -> gid 1..count
  bs::put_u16_be(t, 1);                          // idDelta[1]
  bs::put_u16_be(t, 0);                          // idRangeOffset[0]
  bs::put_u16_be(t, 0);                          // idRangeOffset[1]
  return t;
}

inline std::string cmap_table(const std::uint16_t platform,
                              const std::uint16_t encoding,
                              const std::string &subtable) {
  std::string t;
  bs::put_u16_be(t, 0); // version
  bs::put_u16_be(t, 1); // numTables
  bs::put_u16_be(t, platform);
  bs::put_u16_be(t, encoding);
  bs::put_u32_be(t, 12); // offset to the single subtable
  t += subtable;
  return t;
}

inline std::string name_table(const std::string &ascii) {
  std::string strings;
  for (const char c : ascii) {
    bs::put_u16_be(strings, static_cast<std::uint8_t>(c));
  }
  std::string t;
  bs::put_u16_be(t, 0);     // format
  bs::put_u16_be(t, 1);     // count
  bs::put_u16_be(t, 18);    // stringOffset (6 header + 12 record)
  bs::put_u16_be(t, 3);     // platformID (Windows)
  bs::put_u16_be(t, 1);     // encodingID
  bs::put_u16_be(t, 0x409); // languageID
  bs::put_u16_be(t, 6);     // nameID (PostScript)
  bs::put_u16_be(t, static_cast<std::uint16_t>(strings.size()));
  bs::put_u16_be(t, 0); // offset within string storage
  t += strings;
  return t;
}

inline std::string
sfnt_bytes(const std::vector<std::pair<std::string, std::string>> &tables) {
  const auto count = static_cast<std::uint16_t>(tables.size());
  std::string out;
  bs::put_u32_be(out, 0x00010000); // sfntVersion (TrueType)
  bs::put_u16_be(out, count);
  bs::put_u16_be(out, 0); // searchRange
  bs::put_u16_be(out, 0); // entrySelector
  bs::put_u16_be(out, 0); // rangeShift

  std::uint32_t offset = 12 + count * 16U;
  std::string body;
  for (const auto &[tag, data] : tables) {
    out += tag;
    bs::put_u32_be(out, 0); // checksum (not validated by the reader)
    bs::put_u32_be(out, offset);
    bs::put_u32_be(out, static_cast<std::uint32_t>(data.size()));
    body += data;
    while (body.size() % 4 != 0) {
      body += '\0';
    }
    offset = 12 + count * 16U + static_cast<std::uint32_t>(body.size());
  }
  return out + body;
}

} // namespace odr::test::font
