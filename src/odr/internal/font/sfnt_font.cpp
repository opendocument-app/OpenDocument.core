#include <odr/internal/font/sfnt_font.hpp>

#include <algorithm>
#include <istream>
#include <memory>
#include <utility>

namespace odr::internal::font::sfnt {

namespace {

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

SfntFont::SfntFont(std::unique_ptr<std::istream> stream)
    : m_in{std::move(stream)}, m_parser{*m_in} {
  if (m_in == nullptr) {
    throw std::invalid_argument("sfnt: null input stream");
  }

  m_parser.seek(0);
  const std::string tag = m_parser.read_tag();
  std::uint32_t sfnt_offset = 0;
  if (tag == "ttcf") {
    // TrueType Collection: read the first member's offset table.
    m_parser.seek(12);
    sfnt_offset = m_parser.read_u32();
  }
  m_parser.seek(sfnt_offset);
  read_directory();
  read_head();
  read_maxp();
  read_hhea();
  read_hmtx();
  read_cmap();
  read_name();
}

void SfntFont::read_directory() {
  const std::string version = m_parser.read_tag();
  m_format =
      version == "OTTO" ? FontFormat::opentype_cff : FontFormat::truetype;
  const std::uint16_t num_tables = m_parser.read_u16();

  m_parser.ignore(6); // searchRange, entrySelector, rangeShift

  for (std::uint16_t i = 0; i < num_tables; ++i) {
    std::string tag = m_parser.read_tag();
    m_parser.ignore(4); // checkSum
    const std::uint32_t offset = m_parser.read_u32();
    const std::uint32_t length = m_parser.read_u32();
    m_tables.emplace(std::move(tag), Table{offset, length});
  }
}

void SfntFont::read_head() {
  const auto head = table("head");
  if (!head.has_value()) {
    return; // tolerate; keep the unitsPerEm default
  }
  m_parser.seek(head->offset + 18);
  m_units_per_em = m_parser.read_u16();
  m_parser.seek(head->offset + 36);
  m_bbox = m_parser.read_bbox();
}

void SfntFont::read_maxp() {
  const auto maxp = table("maxp");
  if (!maxp.has_value()) {
    return;
  }
  m_parser.seek(maxp->offset + 4);
  m_glyph_count = m_parser.read_u16();
}

void SfntFont::read_hhea() {
  const auto hhea = table("hhea");
  if (!hhea.has_value()) {
    return;
  }
  m_parser.seek(hhea->offset + 34);
  m_number_of_h_metrics = m_parser.read_u16();
}

void SfntFont::read_hmtx() {
  const auto hmtx = table("hmtx");
  if (!hmtx.has_value()) {
    return;
  }
  m_parser.seek(hmtx->offset);
  m_advance_widths.reserve(m_number_of_h_metrics);
  for (std::uint16_t i = 0; i < m_number_of_h_metrics; ++i) {
    m_advance_widths.push_back(m_parser.read_u16());
    m_parser.ignore(2); // leftSideBearing
  }
}

void SfntFont::read_cmap() {
  const auto cmap = table("cmap");
  if (!cmap.has_value()) {
    return;
  }
  m_parser.seek(cmap->offset + 2);
  const std::uint16_t count = m_parser.read_u16();

  std::vector<CmapEntry> entries;
  entries.reserve(count);
  for (std::uint16_t i = 0; i < count; ++i) {
    entries.emplace_back(m_parser.read_cmap_entry());
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
    m_parser.seek(cmap->offset + best_entry->offset);
    read_cmap_subtable();
  }
}

void SfntFont::read_cmap_subtable() {
  const auto map = [this](const char32_t code, const std::uint16_t glyph) {
    if (glyph == 0) {
      return;
    }
    m_cmap[code] = glyph;
    if (const auto it = m_reverse.find(glyph);
        it == m_reverse.end() || code < it->second) {
      m_reverse[glyph] = code;
    }
  };
  const auto read_u16_vector = [this](const std::size_t count) {
    std::vector<std::uint16_t> out;
    out.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
      out.push_back(m_parser.read_u16());
    }
    return out;
  };

  const std::uint16_t format = m_parser.read_u16();
  switch (static_cast<CmapFormat>(format)) {
  case CmapFormat::byte_encoding: {
    m_parser.ignore(4); // length, language
    for (std::uint32_t code = 0; code < 256; ++code) {
      map(code, m_parser.read_u8());
    }
    break;
  }
  case CmapFormat::segment_mapping: { // segment mapping to delta values
    const std::uint16_t length = m_parser.read_u16();
    m_parser.ignore(2); // language
    const std::size_t segs = m_parser.read_u16() / 2U;
    m_parser.ignore(6); // searchRange, entrySelector, rangeShift
    const std::vector<std::uint16_t> end_codes = read_u16_vector(segs);
    m_parser.ignore(2); // reservedPad
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
    m_parser.ignore(4); // length, language
    const std::uint16_t first = m_parser.read_u16();
    const std::uint16_t entries = m_parser.read_u16();
    for (std::uint16_t i = 0; i < entries; ++i) {
      map(first + i, m_parser.read_u16());
    }
    break;
  }
  case CmapFormat::segmented_coverage: {
    m_parser.ignore(10); // reserved, length, language
    const std::uint32_t groups = m_parser.read_u32();
    for (std::uint32_t g = 0; g < groups; ++g) {
      const std::uint32_t start = m_parser.read_u32();
      const std::uint32_t end = m_parser.read_u32();
      const std::uint32_t start_glyph = m_parser.read_u32();
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
  m_parser.seek(name->offset + 2);
  const std::uint16_t count = m_parser.read_u16();
  const std::uint16_t string_offset = m_parser.read_u16();
  const std::size_t strings = name->offset + string_offset;

  std::vector<NameEntry> entries;
  entries.reserve(count);
  for (std::uint16_t i = 0; i < count; ++i) {
    entries.emplace_back(m_parser.read_name_entry());
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
    m_parser.seek(strings + best_entry->name_local_offset);
    m_name = best_entry->utf16()
                 ? m_parser.read_utf16be(best_entry->name_length)
                 : m_parser.read_latin1(best_entry->name_length);
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

std::optional<SfntFont::Table>
SfntFont::table(const std::string_view tag) const {
  const auto it = m_tables.find(tag);
  if (it == m_tables.end()) {
    return std::nullopt;
  }
  return it->second;
}

} // namespace odr::internal::font::sfnt
