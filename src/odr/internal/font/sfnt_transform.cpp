#include <odr/internal/font/sfnt_transform.hpp>

#include <odr/internal/font/sfnt_font.hpp>

#include <algorithm>
#include <bit>
#include <cstdint>
#include <map>
#include <ostream>
#include <ranges>
#include <stdexcept>
#include <utility>

namespace odr::internal::font {

namespace {

constexpr char32_t pua_base = 0xe000;
constexpr std::uint16_t pua_capacity = 0xf8ff - 0xe000 + 1; // 6400

void put16(std::string &s, const std::uint16_t v) {
  s += static_cast<char>(v >> 8);
  s += static_cast<char>(v & 0xff);
}

void put32(std::string &s, const std::uint32_t v) {
  put16(s, static_cast<std::uint16_t>(v >> 16));
  put16(s, static_cast<std::uint16_t>(v & 0xffff));
}

void write32(std::string &s, const std::size_t at, const std::uint32_t v) {
  s[at] = static_cast<char>(v >> 24);
  s[at + 1] = static_cast<char>((v >> 16) & 0xff);
  s[at + 2] = static_cast<char>((v >> 8) & 0xff);
  s[at + 3] = static_cast<char>(v & 0xff);
}

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

void font::build_sfnt(std::ostream &out, const std::uint32_t sfnt_version,
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
      write32(data, 8, 0);
    }
    const std::uint32_t table_checksum = checksum(data);
    bodies_checksum += table_checksum;
    directory.append(tag);
    put32(directory, table_checksum);
    put32(directory, offset);
    put32(directory, length);
    offset += static_cast<std::uint32_t>(data.size());
  }

  std::string header;
  put32(header, sfnt_version);
  put16(header, count);
  put16(header, search_range);
  put16(header, entry_selector);
  put16(header, range_shift);

  // checksum(file) = checksum(header) + checksum(directory) + Σ checksum(table)
  // because every segment sits on a 4-byte boundary. Patching head with
  // (magic - that sum) makes the realized whole-file checksum the magic.
  if (head_index != tables.size()) {
    const std::uint32_t file_checksum =
        checksum(header) + checksum(directory) + bodies_checksum;
    write32(tables[head_index].second, 8, 0xb1b0afbaU - file_checksum);
  }

  out.write(header.data(), static_cast<std::streamsize>(header.size()));
  out.write(directory.data(), static_cast<std::streamsize>(directory.size()));
  for (const auto &data : tables | std::views::values) {
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
  }
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
  put16(sub, 4); // format
  // length: 7 u16 header + reservedPad + 4 u16 arrays of seg_count entries.
  put16(sub, static_cast<std::uint16_t>(16 + 8 * seg_count));
  put16(sub, 0);                                         // language
  put16(sub, static_cast<std::uint16_t>(seg_count * 2)); // segCountX2
  put16(sub, search_range);
  put16(sub, entry_selector);
  put16(sub, range_shift);
  for (const auto &s : segments) {
    put16(sub, s.end);
  }
  put16(sub, 0); // reservedPad
  for (const auto &s : segments) {
    put16(sub, s.start);
  }
  for (const auto &s : segments) {
    put16(sub, s.delta);
  }
  for (std::uint16_t i = 0; i < seg_count; ++i) {
    put16(sub, 0); // idRangeOffset (always 0: no glyphIdArray)
  }

  std::string cmap;
  put16(cmap, 0);  // version
  put16(cmap, 1);  // numTables
  put16(cmap, 3);  // platformID (Windows)
  put16(cmap, 1);  // encodingID (Unicode BMP)
  put32(cmap, 12); // offset to the subtable
  cmap += sub;
  return cmap;
}

std::string font::serialize_post() {
  std::string post;
  put32(post, 0x00030000); // version 3.0: no glyph names
  put32(post, 0);          // italicAngle
  put16(post, 0);          // underlinePosition
  put16(post, 0);          // underlineThickness
  put32(post, 0);          // isFixedPitch
  put32(post, 0);          // minMemType42
  put32(post, 0);          // maxMemType42
  put32(post, 0);          // minMemType1
  put32(post, 0);          // maxMemType1
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
  put16(os2, 4);                                     // version
  put16(os2, em(500));                               // xAvgCharWidth (estimate)
  put16(os2, 400);                                   // usWeightClass: regular
  put16(os2, 5);                                     // usWidthClass: medium
  put16(os2, 0);                                     // fsType: installable
  put16(os2, em(650));                               // ySubscriptXSize
  put16(os2, em(600));                               // ySubscriptYSize
  put16(os2, 0);                                     // ySubscriptXOffset
  put16(os2, em(75));                                // ySubscriptYOffset
  put16(os2, em(650));                               // ySuperscriptXSize
  put16(os2, em(600));                               // ySuperscriptYSize
  put16(os2, 0);                                     // ySuperscriptXOffset
  put16(os2, em(350));                               // ySuperscriptYOffset
  put16(os2, em(50));                                // yStrikeoutSize
  put16(os2, em(258));                               // yStrikeoutPosition
  put16(os2, 0);                                     // sFamilyClass
  os2.append(10, '\0');                              // panose: any
  put32(os2, 0);                                     // ulUnicodeRange1
  put32(os2, 0);                                     // ulUnicodeRange2
  put32(os2, 0);                                     // ulUnicodeRange3
  put32(os2, 0);                                     // ulUnicodeRange4
  os2.append("ODR ", 4);                             // achVendID
  put16(os2, 0x0040);                                // fsSelection: REGULAR
  put16(os2, first_char);                            // usFirstCharIndex
  put16(os2, last_char);                             // usLastCharIndex
  put16(os2, static_cast<std::uint16_t>(ascender));  // sTypoAscender
  put16(os2, static_cast<std::uint16_t>(descender)); // sTypoDescender
  put16(os2, 0);                                     // sTypoLineGap
  put16(os2, static_cast<std::uint16_t>(ascender));  // usWinAscent
  put16(os2,
        static_cast<std::uint16_t>(-descender)); // usWinDescent (positive)
  put32(os2, 0);                                 // ulCodePageRange1
  put32(os2, 0);                                 // ulCodePageRange2
  put16(os2, 0);                                 // sxHeight
  put16(os2, 0);                                 // sCapHeight
  put16(os2, 0);                                 // usDefaultChar
  put16(os2, 0x20);                              // usBreakChar: space
  put16(os2, 0);                                 // usMaxContext
  return os2;
}

void font::reencode_to_pua(sfnt::SfntFont &font) {
  if (font.glyph_count() > pua_capacity) {
    throw std::runtime_error(
        "sfnt_transform: glyph count exceeds BMP PUA capacity");
  }

  std::map<char32_t, std::uint16_t> map;
  for (std::uint16_t glyph = 0; glyph < font.glyph_count(); ++glyph) {
    map[pua_code_point(glyph)] = glyph;
  }
  font.set_cmap(std::move(map));
}

} // namespace odr::internal
