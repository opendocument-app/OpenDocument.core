#include <odr/internal/font/otf_writer.hpp>

#include <odr/internal/font/sfnt_font.hpp>

#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace odr::internal::font {

namespace {

constexpr char32_t pua_base = 0xe000;
constexpr std::uint16_t pua_capacity = 0xf8ff - 0xe000 + 1; // 6400

void put16(std::string &s, std::uint16_t v) {
  s += static_cast<char>(v >> 8);
  s += static_cast<char>(v & 0xff);
}

void put32(std::string &s, std::uint32_t v) {
  put16(s, static_cast<std::uint16_t>(v >> 16));
  put16(s, static_cast<std::uint16_t>(v & 0xffff));
}

void write32(std::string &s, std::size_t at, std::uint32_t v) {
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

// SFNT checksum: the sum (mod 2^32) of the data read as big-endian uint32
// words, zero-padded to a 4-byte boundary.
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

// A `cmap` with a single (3,1) format-4 subtable mapping U+E000+i -> glyph i
// over the contiguous run of all glyphs (`glyph_count` <= the PUA capacity).
std::string build_pua_cmap(std::uint16_t glyph_count) {
  const auto last = static_cast<std::uint16_t>(pua_base + glyph_count - 1);

  std::string sub;
  put16(sub, 4);      // format
  put16(sub, 32);     // length (14 header + 2 segments * 8 + 2 pad)
  put16(sub, 0);      // language
  put16(sub, 4);      // segCountX2 (2 segments incl. the 0xFFFF terminator)
  put16(sub, 4);      // searchRange   = 2 * 2^floor(log2(segCount))
  put16(sub, 1);      // entrySelector = floor(log2(segCount))
  put16(sub, 0);      // rangeShift    = segCountX2 - searchRange
  put16(sub, last);   // endCode[0]
  put16(sub, 0xffff); // endCode[1]
  put16(sub, 0);      // reservedPad
  put16(sub, static_cast<std::uint16_t>(pua_base)); // startCode[0]
  put16(sub, 0xffff);                               // startCode[1]
  // idDelta[0]: glyph = (code + delta) mod 2^16, so U+E000 -> 0 needs
  // delta = (0x10000 - 0xE000) mod 2^16 = 0x2000 (fits an int16).
  put16(sub, 0x2000); // idDelta[0]
  put16(sub, 1);      // idDelta[1]  (terminator maps 0xFFFF -> 0)
  put16(sub, 0);      // idRangeOffset[0]
  put16(sub, 0);      // idRangeOffset[1]

  std::string cmap;
  put16(cmap, 0);  // version
  put16(cmap, 1);  // numTables
  put16(cmap, 3);  // platformID (Windows)
  put16(cmap, 1);  // encodingID (Unicode BMP)
  put32(cmap, 12); // offset to the subtable
  cmap += sub;
  return cmap;
}

} // namespace

} // namespace odr::internal::font

namespace odr::internal {

char32_t font::pua_code_point(std::uint16_t glyph) noexcept {
  return pua_base + glyph;
}

std::string
font::build_sfnt(std::uint32_t sfnt_version,
                 std::vector<std::pair<std::string, std::string>> tables) {
  std::ranges::sort(
      tables, {}, [](const auto &e) -> const std::string & { return e.first; });

  const auto count = static_cast<std::uint16_t>(tables.size());
  // Largest power of two <= count, for the binary-search hint fields.
  std::uint16_t entry_selector = 0;
  while ((1U << (entry_selector + 1)) <= count) {
    ++entry_selector;
  }
  const auto search_range =
      static_cast<std::uint16_t>((1U << entry_selector) * 16);
  const auto range_shift =
      static_cast<std::uint16_t>(count * 16 - search_range);

  std::string out;
  put32(out, sfnt_version);
  put16(out, count);
  put16(out, search_range);
  put16(out, entry_selector);
  put16(out, range_shift);

  // Reserve the directory; offsets are filled once table positions are known.
  const std::size_t directory = out.size();
  out.resize(directory + static_cast<std::size_t>(count) * 16, '\0');

  std::size_t head_offset = 0;
  for (std::size_t i = 0; i < tables.size(); ++i) {
    auto &[tag, data] = tables[i];
    pad4(out);
    const auto offset = static_cast<std::uint32_t>(out.size());
    if (tag == "head") {
      head_offset = offset;
      // checkSumAdjustment must be zero while checksums are computed.
      if (data.size() >= 12) {
        write32(data, 8, 0);
      }
    }
    const std::size_t entry = directory + i * 16U;
    out.replace(entry, 4, tag);
    write32(out, entry + 4, checksum(data));
    write32(out, entry + 8, offset);
    write32(out, entry + 12, static_cast<std::uint32_t>(data.size()));
    out += data;
  }

  if (head_offset != 0) {
    write32(out, head_offset + 8, 0xb1b0afbaU - checksum(out));
  }
  return out;
}

std::string font::reencode_to_pua(const sfnt::SfntFont &font) {
  if (font.glyph_count() > pua_capacity) {
    throw std::runtime_error(
        "otf_writer: glyph count exceeds BMP PUA capacity");
  }

  std::vector<std::pair<std::string, std::string>> tables;
  for (const std::string &tag : font.table_tags()) {
    if (tag == "cmap") {
      continue; // replaced below
    }
    tables.emplace_back(tag, font.table_data(tag));
  }
  tables.emplace_back("cmap", build_pua_cmap(font.glyph_count()));

  const std::uint32_t version = font.format() == FontFormat::opentype_cff
                                    ? 0x4f54544fU /* 'OTTO' */
                                    : 0x00010000U;
  return build_sfnt(version, std::move(tables));
}

} // namespace odr::internal
