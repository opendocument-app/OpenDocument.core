#include <odr/internal/font/sfnt_font.hpp>

#include <odr/internal/font/sfnt_transform.hpp>
#include <odr/internal/util/byte_string.hpp>
#include <odr/internal/util/stream_util.hpp>

#include <algorithm>
#include <cstdint>
#include <istream>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace odr::internal::font::sfnt {

namespace bs = util::byte_string;

namespace {

// SFNT enumerations (OpenType spec). Values are the on-disk codes; casting a
// raw `u16` to one and switching/comparing keeps the magic numbers in one
// place.

/// `cmap`/`name` platform IDs.
enum class PlatformId : std::uint16_t {
  unicode = 0,
  macintosh = 1,
  windows = 3,
};

/// Windows-platform (`PlatformId::windows`) encoding IDs.
enum class WindowsEncoding : std::uint16_t {
  symbol = 0,
  unicode_bmp = 1,
  unicode_full = 10,
};

/// Unicode-platform (`PlatformId::unicode`) encoding IDs that name a full
/// (beyond-BMP) repertoire; the rest are treated as plain Unicode.
enum class UnicodeEncoding : std::uint16_t {
  unicode_2_0_full = 4,
  unicode_full = 6,
};

/// Macintosh-platform (`PlatformId::macintosh`) encoding IDs.
enum class MacintoshEncoding : std::uint16_t {
  roman = 0,
};

/// `cmap` subtable format (the leading `u16` of a subtable).
enum class CmapFormat : std::uint16_t {
  byte_encoding = 0,      // format 0
  segment_mapping = 4,    // format 4
  trimmed_mapping = 6,    // format 6
  segmented_coverage = 12 // format 12
};

/// `name` record IDs we extract a font name from.
enum class NameId : std::uint16_t {
  full = 4,
  postscript = 6,
};

struct CmapEntry {
  PlatformId platform;
  std::uint16_t encoding;
  std::uint32_t offset;
};

struct NameEntry {
  PlatformId platform;
  NameId name_id;
  std::uint16_t name_length;
  std::uint16_t name_local_offset;

  [[nodiscard]] bool utf16() const {
    return platform == PlatformId::windows || platform == PlatformId::unicode;
  }
};

void append_utf8(std::string &out, const char32_t cp) {
  if (cp < 0x80) {
    out += static_cast<char>(cp);
  } else if (cp < 0x800) {
    out += static_cast<char>(0xc0 | (cp >> 6));
    out += static_cast<char>(0x80 | (cp & 0x3f));
  } else if (cp < 0x10000) {
    out += static_cast<char>(0xe0 | (cp >> 12));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
    out += static_cast<char>(0x80 | (cp & 0x3f));
  } else {
    out += static_cast<char>(0xf0 | (cp >> 18));
    out += static_cast<char>(0x80 | ((cp >> 12) & 0x3f));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
    out += static_cast<char>(0x80 | (cp & 0x3f));
  }
}

// The decoders below read a fixed record from the **front** of @p d (the view
// is positioned at the record by the caller); the remaining bytes are ignored.

/// `head`'s bounding box: `xMin`/`yMin`/`xMax`/`yMax` as four `int16`.
[[nodiscard]] FontBBox read_bbox(const std::string_view d) {
  return {static_cast<std::int16_t>(bs::read_u16_be(d)),
          static_cast<std::int16_t>(bs::read_u16_be(d.substr(2))),
          static_cast<std::int16_t>(bs::read_u16_be(d.substr(4))),
          static_cast<std::int16_t>(bs::read_u16_be(d.substr(6)))};
}

/// A `cmap` encoding record: platformID, encodingID, subtable offset.
[[nodiscard]] CmapEntry read_cmap_entry(const std::string_view d) {
  return {static_cast<PlatformId>(bs::read_u16_be(d)),
          bs::read_u16_be(d.substr(2)), bs::read_u32_be(d.substr(4))};
}

/// A `name` record; encodingID/languageID (+2..+6) are skipped.
[[nodiscard]] NameEntry read_name_entry(const std::string_view d) {
  return {static_cast<PlatformId>(bs::read_u16_be(d)),
          static_cast<NameId>(bs::read_u16_be(d.substr(6))),
          bs::read_u16_be(d.substr(8)), bs::read_u16_be(d.substr(10))};
}

/// Decode a UTF-16BE `name` string (Windows/Unicode platforms), surrogate pairs
/// included, into UTF-8. @p d is exactly the string's bytes.
[[nodiscard]] std::string read_utf16be(const std::string_view d) {
  const std::size_t len = d.size();
  std::string out;
  for (std::size_t i = 0; i + 1 < len; i += 2) {
    char32_t cp = bs::read_u16_be(d.substr(i));
    if (cp >= 0xd800 && cp <= 0xdbff && i + 3 < len) {
      const char32_t lo = bs::read_u16_be(d.substr(i + 2));
      if (lo >= 0xdc00 && lo <= 0xdfff) {
        cp = 0x10000 + ((cp - 0xd800) << 10) + (lo - 0xdc00);
        i += 2;
      }
    }
    append_utf8(out, cp);
  }
  return out;
}

/// Decode a single-byte (Mac/Latin-1) `name` string into UTF-8. @p d is exactly
/// the string's bytes.
[[nodiscard]] std::string read_latin1(const std::string_view d) {
  std::string out;
  for (const char c : d) {
    append_utf8(out, static_cast<std::uint8_t>(c));
  }
  return out;
}

/// Preference among `cmap` subtables: Unicode full > Unicode BMP > symbol >
/// Mac.
[[nodiscard]] int cmap_score(const PlatformId platform,
                             const std::uint16_t encoding) {
  switch (platform) {
  case PlatformId::windows:
    switch (static_cast<WindowsEncoding>(encoding)) {
    case WindowsEncoding::unicode_full:
      return 5;
    case WindowsEncoding::unicode_bmp:
      return 4;
    case WindowsEncoding::symbol:
      return 2;
    }
    return 0;
  case PlatformId::unicode:
    if (static_cast<UnicodeEncoding>(encoding) ==
            UnicodeEncoding::unicode_2_0_full ||
        static_cast<UnicodeEncoding>(encoding) ==
            UnicodeEncoding::unicode_full) {
      return 5;
    }
    return 3;
  case PlatformId::macintosh:
    // Mac Roman is treated as Latin-1 — correct for ASCII.
    return static_cast<MacintoshEncoding>(encoding) == MacintoshEncoding::roman
               ? 1
               : 0;
  }
  return 0;
}

} // namespace

bool SfntFont::is_sfnt(const std::string_view data) {
  if (data.size() < 4) {
    return false;
  }
  const std::string_view tag = data.substr(0, 4);
  return tag == std::string_view("\x00\x01\x00\x00", 4) || tag == "OTTO" ||
         tag == "true" || tag == "ttcf" || tag == "typ1";
}

SfntFont::SfntFont(std::unique_ptr<std::istream> stream) {
  if (stream == nullptr) {
    throw std::invalid_argument("sfnt: null input stream");
  }
  m_data = util::stream::read(*stream);
  parse();
}

SfntFont::SfntFont(std::string data) : m_data{std::move(data)} { parse(); }

void SfntFont::parse() {
  const std::string_view d{m_data};

  std::uint32_t sfnt_offset = 0;
  if (d.substr(0, 4) == "ttcf") {
    // TrueType Collection: read the first member's offset table.
    sfnt_offset = bs::read_u32_be(d.substr(12));
  }
  read_directory(d.substr(sfnt_offset));
  read_head();
  read_maxp();
  read_hhea();
  read_hmtx();
  read_cmap();
  read_name();
}

std::string_view SfntFont::table_data(const Table table) const {
  return std::string_view{m_data}.substr(table.offset, table.length);
}

void SfntFont::read_directory(const std::string_view sfnt) {
  m_format = sfnt.substr(0, 4) == "OTTO" ? FontFormat::opentype_cff
                                         : FontFormat::truetype;
  const std::uint16_t num_tables = bs::read_u16_be(sfnt.substr(4));

  // The offset table is 12 bytes (sfntVersion, numTables, then the three search
  // hints); each of the `num_tables` directory entries is 16 bytes: tag(4),
  // checkSum(4), offset(4), length(4).
  for (std::uint16_t i = 0; i < num_tables; ++i) {
    const std::size_t entry = 12 + static_cast<std::size_t>(i) * 16;
    std::string tag{sfnt.substr(entry, 4)};
    const std::uint32_t offset = bs::read_u32_be(sfnt.substr(entry + 8));
    const std::uint32_t length = bs::read_u32_be(sfnt.substr(entry + 12));
    m_tables.emplace(std::move(tag), Table{offset, length});
  }
}

void SfntFont::read_head() {
  const auto head = table("head");
  if (!head.has_value()) {
    return; // tolerate; keep the unitsPerEm default
  }
  const std::string_view t = table_data(*head);
  m_units_per_em = bs::read_u16_be(t.substr(18));
  m_bbox = read_bbox(t.substr(36));
}

void SfntFont::read_maxp() {
  const auto maxp = table("maxp");
  if (!maxp.has_value()) {
    return;
  }
  m_glyph_count = bs::read_u16_be(table_data(*maxp).substr(4));
}

void SfntFont::read_hhea() {
  const auto hhea = table("hhea");
  if (!hhea.has_value()) {
    return;
  }
  m_number_of_h_metrics = bs::read_u16_be(table_data(*hhea).substr(34));
}

void SfntFont::read_hmtx() {
  const auto hmtx = table("hmtx");
  if (!hmtx.has_value()) {
    return;
  }
  const std::string_view t = table_data(*hmtx);
  m_advance_widths.reserve(m_number_of_h_metrics);
  for (std::uint16_t i = 0; i < m_number_of_h_metrics; ++i) {
    // Each longHorMetric is advanceWidth(2) + leftSideBearing(2).
    m_advance_widths.push_back(
        bs::read_u16_be(t.substr(static_cast<std::size_t>(i) * 4)));
  }
}

void SfntFont::read_cmap() {
  const auto cmap = table("cmap");
  if (!cmap.has_value()) {
    return;
  }
  const std::string_view t = table_data(*cmap);
  const std::uint16_t count = bs::read_u16_be(t.substr(2));

  std::vector<CmapEntry> entries;
  entries.reserve(count);
  for (std::uint16_t i = 0; i < count; ++i) {
    // The encoding records start at offset 4, 8 bytes each.
    entries.emplace_back(
        read_cmap_entry(t.substr(4 + static_cast<std::size_t>(i) * 8)));
  }

  m_symbolic = std::ranges::any_of(entries, [](const CmapEntry &entry) {
    return entry.platform == PlatformId::windows &&
           static_cast<WindowsEncoding>(entry.encoding) ==
               WindowsEncoding::symbol;
  });

  const auto best_entry =
      std::ranges::max_element(entries, {}, [](const CmapEntry &entry) {
        return cmap_score(entry.platform, entry.encoding);
      });

  if (best_entry != entries.end() &&
      cmap_score(best_entry->platform, best_entry->encoding) > 0) {
    // The encoding record's offset is relative to the start of the `cmap`
    // table.
    read_cmap_subtable(t.substr(best_entry->offset));
  }

  update_reverse();
}

void SfntFont::update_reverse() {
  // Lowest code point wins; m_cmap iterates code points ascending, so the
  // first time a glyph is seen is its lowest code point.
  m_reverse.clear();
  for (const auto &[code, glyph] : m_cmap) {
    m_reverse.emplace(glyph, code);
  }
}

void SfntFont::read_cmap_subtable(const std::string_view subtable) {
  const auto map = [this](const char32_t code, const std::uint16_t glyph) {
    if (glyph == 0) {
      return;
    }
    m_cmap[code] = glyph;
  };

  // A forward cursor over the subtable: each read consumes from the front.
  std::string_view c = subtable;
  const auto u8 = [&c] {
    const std::uint8_t v = bs::read_u8(c);
    c.remove_prefix(1);
    return v;
  };
  const auto u16 = [&c] {
    const std::uint16_t v = bs::read_u16_be(c);
    c.remove_prefix(2);
    return v;
  };
  const auto u32 = [&c] {
    const std::uint32_t v = bs::read_u32_be(c);
    c.remove_prefix(4);
    return v;
  };
  const auto skip = [&c](const std::size_t n) { c.remove_prefix(n); };
  const auto read_u16_vector = [&u16](const std::size_t count) {
    std::vector<std::uint16_t> out;
    out.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
      out.push_back(u16());
    }
    return out;
  };

  switch (static_cast<CmapFormat>(u16())) {
  case CmapFormat::byte_encoding: {
    skip(4); // length, language
    for (std::uint32_t code = 0; code < 256; ++code) {
      map(code, u8());
    }
    break;
  }
  case CmapFormat::segment_mapping: { // segment mapping to delta values
    const std::uint16_t length = u16();
    skip(2); // language
    const std::size_t segs = u16() / 2U;
    skip(6); // searchRange, entrySelector, rangeShift
    const std::vector<std::uint16_t> end_codes = read_u16_vector(segs);
    skip(2); // reservedPad
    const std::vector<std::uint16_t> start_codes = read_u16_vector(segs);
    const std::vector<std::uint16_t> id_deltas = read_u16_vector(segs);
    const std::vector<std::uint16_t> id_range_offsets = read_u16_vector(segs);
    // Whatever remains of the subtable is the glyphIdArray that non-zero
    // idRangeOffsets index into; preload it so the inner loop is a plain
    // lookup. The header up to this point is 16 + 8*segs bytes.
    const std::size_t header = 16 + 8 * segs;
    if (length < header) {
      throw std::runtime_error("sfnt: cmap format 4 subtable too short");
    }
    const std::vector<std::uint16_t> glyph_ids =
        read_u16_vector((length - header) / 2);
    for (std::size_t s = 0; s < segs; ++s) {
      const std::uint16_t start = start_codes.at(s);
      const std::uint16_t end = end_codes.at(s);
      const std::uint16_t delta = id_deltas.at(s);
      const std::uint16_t range = id_range_offsets.at(s);
      for (std::uint32_t code = start; code <= end && code != 0xffff; ++code) {
        std::uint16_t glyph = 0;
        if (range == 0) {
          glyph = static_cast<std::uint16_t>(code + delta);
        } else {
          // idRangeOffset is a self-relative byte offset from
          // &idRangeOffset[s]; as an index into glyphIdArray it is range/2 +
          // (code-start) - (segs-s).
          const std::size_t index = range / 2U + (code - start) - (segs - s);
          glyph = glyph_ids.at(index);
          if (glyph != 0) {
            glyph = static_cast<std::uint16_t>(glyph + delta);
          }
        }
        map(code, glyph);
      }
    }
    break;
  }
  case CmapFormat::trimmed_mapping: {
    skip(4); // length, language
    const std::uint16_t first = u16();
    const std::uint16_t entries = u16();
    for (std::uint16_t i = 0; i < entries; ++i) {
      map(first + i, u16());
    }
    break;
  }
  case CmapFormat::segmented_coverage: {
    skip(10); // reserved, length, language
    const std::uint32_t groups = u32();
    for (std::uint32_t g = 0; g < groups; ++g) {
      const std::uint32_t start = u32();
      const std::uint32_t end = u32();
      const std::uint32_t start_glyph = u32();
      for (std::uint32_t code = start; code <= end; ++code) {
        map(code, static_cast<std::uint16_t>(start_glyph + (code - start)));
      }
    }
    break;
  }
  default:
    break; // unsupported subtable format: leave the map empty
  }
}

void SfntFont::read_name() {
  const auto name = table("name");
  if (!name.has_value()) {
    return;
  }
  const std::string_view t = table_data(*name);
  const std::uint16_t count = bs::read_u16_be(t.substr(2));
  const std::uint16_t string_offset = bs::read_u16_be(t.substr(4));

  std::vector<NameEntry> entries;
  entries.reserve(count);
  for (std::uint16_t i = 0; i < count; ++i) {
    // The name records start at offset 6, 12 bytes each.
    entries.emplace_back(
        read_name_entry(t.substr(6 + static_cast<std::size_t>(i) * 12)));
  }

  const auto score_entry = [](const NameEntry &entry) {
    // Only the full (4) and PostScript (6) names are usable; everything else
    // scores below the empty default so it is never selected.
    if (entry.name_id != NameId::full && entry.name_id != NameId::postscript) {
      return 0;
    }
    const int score = (entry.name_id == NameId::postscript ? 10 : 0) +
                      (entry.utf16() ? 2 : 1);
    return score;
  };

  const auto best_entry = std::ranges::max_element(entries, {}, score_entry);

  if (best_entry != entries.end() && score_entry(*best_entry) > 0) {
    // name_local_offset is relative to the string storage (string_offset).
    const std::string_view value = t.substr(
        static_cast<std::size_t>(string_offset) + best_entry->name_local_offset,
        best_entry->name_length);
    m_name = best_entry->utf16() ? read_utf16be(value) : read_latin1(value);
  }
}

FontFormat SfntFont::format() const noexcept { return m_format; }

std::string SfntFont::name() const { return m_name; }

std::uint16_t SfntFont::glyph_count() const noexcept { return m_glyph_count; }

std::uint16_t SfntFont::units_per_em() const noexcept { return m_units_per_em; }

bool SfntFont::symbolic() const noexcept { return m_symbolic; }

FontBBox SfntFont::bounding_box() const noexcept { return m_bbox; }

std::uint16_t SfntFont::advance_width(const std::uint16_t glyph) const {
  if (m_advance_widths.empty()) {
    return 0;
  }
  // Glyphs beyond `numberOfHMetrics` share the last advance (monospace tail).
  if (glyph >= m_advance_widths.size()) {
    return m_advance_widths.back();
  }
  return m_advance_widths[glyph];
}

std::uint16_t SfntFont::glyph_for_code_point(const char32_t code_point) const {
  const auto it = m_cmap.find(code_point);
  return it == m_cmap.end() ? 0 : it->second;
}

std::optional<char32_t>
SfntFont::code_point_for_glyph(const std::uint16_t glyph) const {
  const auto it = m_reverse.find(glyph);
  if (it == m_reverse.end()) {
    return std::nullopt;
  }
  return it->second;
}

const std::map<char32_t, std::uint16_t> &SfntFont::cmap() const noexcept {
  return m_cmap;
}

void SfntFont::set_cmap(std::map<char32_t, std::uint16_t> cmap) {
  m_cmap = std::move(cmap);
  update_reverse();
}

void SfntFont::write(std::ostream &out) const {
  std::vector<std::pair<std::string, std::string>> tables;
  tables.reserve(m_tables.size() + 1);
  for (const auto &[tag, location] : m_tables) {
    if (tag == "cmap") {
      continue; // rebuilt from the cmap() model below
    }
    tables.emplace_back(tag, m_data.substr(location.offset, location.length));
  }
  tables.emplace_back("cmap", serialize_cmap(m_cmap));

  // A `post` table is required by OTS; PDF-embedded TrueType fonts often omit
  // it. Synthesize a minimal one so the browser accepts the `@font-face`.
  if (!m_tables.contains("post")) {
    tables.emplace_back("post", serialize_post());
  }

  // `OS/2` is likewise required by OTS and likewise often omitted. Synthesize
  // it from the cmap bounds and bounding box (build_sfnt sorts the directory,
  // so the insertion order here does not matter).
  if (!m_tables.contains("OS/2")) {
    std::uint16_t first_char = 0;
    std::uint16_t last_char = 0;
    if (!m_cmap.empty()) {
      first_char = static_cast<std::uint16_t>(
          std::min<char32_t>(m_cmap.begin()->first, 0xffff));
      last_char = static_cast<std::uint16_t>(
          std::min<char32_t>(m_cmap.rbegin()->first, 0xffff));
    }
    tables.emplace_back("OS/2",
                        serialize_os2(m_units_per_em, m_bbox.y_min,
                                      m_bbox.y_max, first_char, last_char));
  }

  const std::uint32_t version = m_format == FontFormat::opentype_cff
                                    ? 0x4f54544fU /* 'OTTO' */
                                    : 0x00010000U;
  build_sfnt(out, version, std::move(tables));
}

std::optional<SfntFont::Table>
SfntFont::table(const std::string_view tag) const {
  const auto it = m_tables.find(tag);
  if (it == m_tables.end()) {
    return std::nullopt;
  }
  return it->second;
}

} // namespace odr::internal::font::sfnt
