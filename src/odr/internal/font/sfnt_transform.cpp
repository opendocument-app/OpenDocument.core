#include <odr/internal/font/sfnt_transform.hpp>

#include <odr/internal/font/sfnt_font.hpp>
#include <odr/internal/util/byte_string.hpp>

#include <algorithm>
#include <bit>
#include <cstdint>
#include <map>
#include <ranges>
#include <stdexcept>
#include <utility>

namespace odr::internal::font {

namespace {

namespace bs = util::byte_string;

constexpr char32_t pua_base = 0xe000;
constexpr std::uint16_t pua_capacity = 0xf8ff - 0xe000 + 1; // 6400

void pad4(std::string &s) {
  while (s.size() % 4 != 0) {
    s += '\0';
  }
}

/// SFNT checksum: the sum (mod 2^32) of the data read as big-endian uint32
/// words, zero-padded to a 4-byte boundary.
std::uint32_t checksum(const std::string &data) {
  std::uint32_t sum = 0;
  for (std::size_t i = 0; i < data.size(); i += 4) {
    std::uint32_t word = 0;
    for (std::size_t b = 0; b < 4; ++b) {
      word = word << 8 |
             (i + b < data.size() ? static_cast<std::uint8_t>(data[i + b]) : 0);
    }
    sum += word;
  }
  return sum;
}

/// The binary-search hint fields for a directory/subtable of @p count entries:
/// `entrySelector = floor(log2(count))`, `searchRange = unit *
/// 2^entrySelector`, `rangeShift = count * unit - searchRange` (`unit` is 16
/// for the table directory, 2 for a format-4 segment array).
struct SearchHints {
  std::uint16_t search_range;
  std::uint16_t entry_selector;
  std::uint16_t range_shift;
};
SearchHints search_hints(const std::uint16_t count, const std::uint16_t unit) {
  const auto entry_selector =
      static_cast<std::uint16_t>(std::bit_width(count) - 1);
  const auto search_range =
      static_cast<std::uint16_t>((1U << entry_selector) * unit);
  const auto range_shift =
      static_cast<std::uint16_t>(count * unit - search_range);
  return {search_range, entry_selector, range_shift};
}

} // namespace

} // namespace odr::internal::font

namespace odr::internal {

char32_t font::pua_code_point(const std::uint16_t glyph) noexcept {
  return pua_base + glyph;
}

std::string
font::build_sfnt(const std::uint32_t sfnt_version,
                 std::vector<std::pair<std::string, std::string>> tables) {
  std::ranges::sort(
      tables, {}, [](const auto &e) -> const std::string & { return e.first; });

  const auto count = static_cast<std::uint16_t>(tables.size());
  const auto [search_range, entry_selector, range_shift] =
      search_hints(count, 16);

  constexpr std::size_t header_size = 12;
  const auto offset_table_size =
      header_size + static_cast<std::size_t>(count) * 16;

  // Lay the tables out (each padded to a 4-byte boundary), accumulating the
  // directory and the running checksum of the table bodies. The directory
  // `length` is the unpadded table length (per spec); the checksum and the
  // next offset use the padded data.
  std::string directory;
  directory.reserve(static_cast<std::size_t>(count) * 16);
  auto offset = static_cast<std::uint32_t>(offset_table_size);
  std::size_t head_index = tables.size(); // sentinel: no head
  std::uint32_t bodies_checksum = 0;
  for (std::size_t i = 0; i < tables.size(); ++i) {
    auto &[tag, data] = tables[i];
    const auto length = static_cast<std::uint32_t>(data.size());
    pad4(data);
    if (tag == "head" && data.size() >= 12) {
      head_index = i;
      // checkSumAdjustment must be zero while checksums are computed.
      bs::write_u32_be(data, 8, 0);
    }
    const std::uint32_t table_checksum = checksum(data);
    bodies_checksum += table_checksum;
    directory.append(tag);
    bs::put_u32_be(directory, table_checksum);
    bs::put_u32_be(directory, offset);
    bs::put_u32_be(directory, length);
    offset += static_cast<std::uint32_t>(data.size());
  }

  std::string header;
  bs::put_u32_be(header, sfnt_version);
  bs::put_u16_be(header, count);
  bs::put_u16_be(header, search_range);
  bs::put_u16_be(header, entry_selector);
  bs::put_u16_be(header, range_shift);

  // checksum(file) = checksum(header) + checksum(directory) + Σ checksum(table)
  // because every segment sits on a 4-byte boundary. Patching head with
  // (magic - that sum) makes the realized whole-file checksum the magic.
  if (head_index != tables.size()) {
    const std::uint32_t file_checksum =
        checksum(header) + checksum(directory) + bodies_checksum;
    bs::write_u32_be(tables[head_index].second, 8, 0xb1b0afbaU - file_checksum);
  }

  std::string out;
  out.reserve(offset); // `offset` has accumulated to the whole-file size
  out.append(header);
  out.append(directory);
  for (const auto &data : tables | std::views::values) {
    out.append(data);
  }
  return out;
}

std::string font::serialize_cmap(const std::map<char32_t, std::uint16_t> &map) {
  // A format-4 segment: a contiguous code range [start, end] whose glyph is
  // `code + delta` (mod 2^16), i.e. idRangeOffset = 0.
  struct Segment {
    std::uint16_t start;
    std::uint16_t end;
    std::uint16_t delta;
  };
  std::vector<Segment> segments;
  for (const auto &[code, glyph] : map) {
    if (code > 0xffff) {
      throw std::runtime_error(
          "sfnt: serialize_cmap supports only BMP code points (format 4); "
          "beyond-BMP coverage (format 12) is a follow-up");
    }
    const auto c = static_cast<std::uint16_t>(code);
    const auto delta = static_cast<std::uint16_t>(glyph - c);
    if (!segments.empty() && c == segments.back().end + 1 &&
        delta == segments.back().delta) {
      segments.back().end = c; // extend the current arithmetic run
    } else {
      segments.push_back({c, c, delta});
    }
  }
  // Mandatory terminator segment: 0xFFFF -> 0 (delta 1).
  segments.push_back({0xffff, 0xffff, 1});

  const auto seg_count = static_cast<std::uint16_t>(segments.size());
  const auto [search_range, entry_selector, range_shift] =
      search_hints(seg_count, 2);

  std::string sub;
  bs::put_u16_be(sub, 4); // format
  // length: 7 u16 header + reservedPad + 4 u16 arrays of seg_count entries.
  bs::put_u16_be(sub, static_cast<std::uint16_t>(16 + 8 * seg_count));
  bs::put_u16_be(sub, 0);                                         // language
  bs::put_u16_be(sub, static_cast<std::uint16_t>(seg_count * 2)); // segCountX2
  bs::put_u16_be(sub, search_range);
  bs::put_u16_be(sub, entry_selector);
  bs::put_u16_be(sub, range_shift);
  for (const auto &s : segments) {
    bs::put_u16_be(sub, s.end);
  }
  bs::put_u16_be(sub, 0); // reservedPad
  for (const auto &s : segments) {
    bs::put_u16_be(sub, s.start);
  }
  for (const auto &s : segments) {
    bs::put_u16_be(sub, s.delta);
  }
  for (std::uint16_t i = 0; i < seg_count; ++i) {
    bs::put_u16_be(sub, 0); // idRangeOffset (always 0: no glyphIdArray)
  }

  std::string cmap;
  bs::put_u16_be(cmap, 0);  // version
  bs::put_u16_be(cmap, 1);  // numTables
  bs::put_u16_be(cmap, 3);  // platformID (Windows)
  bs::put_u16_be(cmap, 1);  // encodingID (Unicode BMP)
  bs::put_u32_be(cmap, 12); // offset to the subtable
  cmap += sub;
  return cmap;
}

std::string font::serialize_post() {
  std::string post;
  bs::put_u32_be(post, 0x00030000); // version 3.0: no glyph names
  bs::put_u32_be(post, 0);          // italicAngle
  bs::put_u16_be(post, 0);          // underlinePosition
  bs::put_u16_be(post, 0);          // underlineThickness
  bs::put_u32_be(post, 0);          // isFixedPitch
  bs::put_u32_be(post, 0);          // minMemType42
  bs::put_u32_be(post, 0);          // maxMemType42
  bs::put_u32_be(post, 0);          // minMemType1
  bs::put_u32_be(post, 0);          // maxMemType1
  return post;
}

std::string font::serialize_os2(const std::uint16_t units_per_em,
                                const std::int16_t y_min,
                                const std::int16_t y_max,
                                const std::uint16_t first_char,
                                const std::uint16_t last_char) {
  // Scale a default expressed against a 1000-unit em to this font's em.
  const auto em = [&](const int per_1000) {
    return static_cast<std::uint16_t>(per_1000 * units_per_em / 1000);
  };
  // Ascender/descender from the bounding box, falling back to 0.8/0.2 em when
  // the box is degenerate so the line metrics are never zero.
  const auto ascender = static_cast<std::int16_t>(y_max > 0 ? y_max : em(800));
  const auto descender =
      static_cast<std::int16_t>(y_min < 0 ? y_min : -em(200));

  std::string os2;
  bs::put_u16_be(os2, 4);          // version
  bs::put_u16_be(os2, em(500));    // xAvgCharWidth (estimate)
  bs::put_u16_be(os2, 400);        // usWeightClass: regular
  bs::put_u16_be(os2, 5);          // usWidthClass: medium
  bs::put_u16_be(os2, 0);          // fsType: installable
  bs::put_u16_be(os2, em(650));    // ySubscriptXSize
  bs::put_u16_be(os2, em(600));    // ySubscriptYSize
  bs::put_u16_be(os2, 0);          // ySubscriptXOffset
  bs::put_u16_be(os2, em(75));     // ySubscriptYOffset
  bs::put_u16_be(os2, em(650));    // ySuperscriptXSize
  bs::put_u16_be(os2, em(600));    // ySuperscriptYSize
  bs::put_u16_be(os2, 0);          // ySuperscriptXOffset
  bs::put_u16_be(os2, em(350));    // ySuperscriptYOffset
  bs::put_u16_be(os2, em(50));     // yStrikeoutSize
  bs::put_u16_be(os2, em(258));    // yStrikeoutPosition
  bs::put_u16_be(os2, 0);          // sFamilyClass
  os2.append(10, '\0');            // panose: any
  bs::put_u32_be(os2, 0);          // ulUnicodeRange1
  bs::put_u32_be(os2, 0);          // ulUnicodeRange2
  bs::put_u32_be(os2, 0);          // ulUnicodeRange3
  bs::put_u32_be(os2, 0);          // ulUnicodeRange4
  os2.append("ODR ", 4);           // achVendID
  bs::put_u16_be(os2, 0x0040);     // fsSelection: REGULAR
  bs::put_u16_be(os2, first_char); // usFirstCharIndex
  bs::put_u16_be(os2, last_char);  // usLastCharIndex
  bs::put_u16_be(os2, static_cast<std::uint16_t>(ascender));  // sTypoAscender
  bs::put_u16_be(os2, static_cast<std::uint16_t>(descender)); // sTypoDescender
  bs::put_u16_be(os2, 0);                                     // sTypoLineGap
  bs::put_u16_be(os2, static_cast<std::uint16_t>(ascender));  // usWinAscent
  bs::put_u16_be(
      os2,
      static_cast<std::uint16_t>(-descender)); // usWinDescent (positive)
  bs::put_u32_be(os2, 0);                      // ulCodePageRange1
  bs::put_u32_be(os2, 0);                      // ulCodePageRange2
  bs::put_u16_be(os2, 0);                      // sxHeight
  bs::put_u16_be(os2, 0);                      // sCapHeight
  bs::put_u16_be(os2, 0);                      // usDefaultChar
  bs::put_u16_be(os2, 0x20);                   // usBreakChar: space
  bs::put_u16_be(os2, 0);                      // usMaxContext
  return os2;
}

void font::reencode_to_pua(sfnt::SfntFont &font,
                           const std::map<char32_t, std::uint16_t> &extra) {
  if (font.glyph_count() > pua_capacity) {
    throw std::runtime_error(
        "sfnt_transform: glyph count exceeds BMP PUA capacity");
  }

  std::map<char32_t, std::uint16_t> map;
  for (std::uint16_t glyph = 0; glyph < font.glyph_count(); ++glyph) {
    map[pua_code_point(glyph)] = glyph;
  }
  // Real-Unicode entries: caller guarantees BMP, non-PUA keys, so these never
  // collide with the PUA range filled above. A glyph id the font does not have
  // is dropped: `glyph_for_code` can fall back to "code as GID" (ISO 32000-1
  // 9.6.6.4) and yield an out-of-range index, and a single cmap reference past
  // `numGlyphs` makes the OTS sanitizer reject the *entire* font (so every
  // glyph would render as a tofu box, not just the unmappable code).
  for (const auto &[code, glyph] : extra) {
    if (glyph < font.glyph_count()) {
      map[code] = glyph;
    }
  }
  font.set_cmap(std::move(map));
}

} // namespace odr::internal
