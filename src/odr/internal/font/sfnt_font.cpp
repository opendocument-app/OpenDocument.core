#include <odr/internal/font/sfnt_font.hpp>

#include <stdexcept>

namespace odr::internal::font {

namespace {

// All SFNT integers are big-endian; these read with bounds checks so a
// truncated/garbage font throws rather than reads out of bounds.
[[nodiscard]] std::uint8_t u8(const std::string &d, std::size_t o) {
  if (o + 1 > d.size()) {
    throw std::runtime_error("sfnt: read past end");
  }
  return static_cast<std::uint8_t>(d[o]);
}

[[nodiscard]] std::uint16_t u16(const std::string &d, std::size_t o) {
  return static_cast<std::uint16_t>(u8(d, o) << 8 | u8(d, o + 1));
}

[[nodiscard]] std::int16_t s16(const std::string &d, std::size_t o) {
  return static_cast<std::int16_t>(u16(d, o));
}

[[nodiscard]] std::uint32_t u32(const std::string &d, std::size_t o) {
  return static_cast<std::uint32_t>(u16(d, o)) << 16 | u16(d, o + 2);
}

[[nodiscard]] std::string tag_at(const std::string &d, std::size_t o) {
  if (o + 4 > d.size()) {
    throw std::runtime_error("sfnt: read past end");
  }
  return d.substr(o, 4);
}

void append_utf8(std::string &out, char32_t cp) {
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

// Decode a UTF-16BE `name` string (Windows/Unicode platforms), surrogate pairs
// included, into UTF-8.
[[nodiscard]] std::string decode_utf16be(const std::string &d, std::size_t o,
                                         std::size_t len) {
  std::string out;
  for (std::size_t i = 0; i + 1 < len; i += 2) {
    char32_t cp = u16(d, o + i);
    if (cp >= 0xd800 && cp <= 0xdbff && i + 3 < len) {
      const char32_t lo = u16(d, o + i + 2);
      if (lo >= 0xdc00 && lo <= 0xdfff) {
        cp = 0x10000 + ((cp - 0xd800) << 10) + (lo - 0xdc00);
        i += 2;
      }
    }
    append_utf8(out, cp);
  }
  return out;
}

// Decode a single-byte (Mac/Latin-1) `name` string into UTF-8.
[[nodiscard]] std::string decode_latin1(const std::string &d, std::size_t o,
                                        std::size_t len) {
  std::string out;
  for (std::size_t i = 0; i < len; ++i) {
    append_utf8(out, u8(d, o + i));
  }
  return out;
}

// Preference among `cmap` subtables: Unicode full > Unicode BMP > symbol > Mac.
[[nodiscard]] int cmap_score(std::uint16_t platform, std::uint16_t encoding) {
  if (platform == 3 && encoding == 10) {
    return 5;
  }
  if (platform == 0 && (encoding == 4 || encoding == 6)) {
    return 5;
  }
  if (platform == 3 && encoding == 1) {
    return 4;
  }
  if (platform == 0) {
    return 3;
  }
  if (platform == 3 && encoding == 0) {
    return 2; // symbol
  }
  if (platform == 1 && encoding == 0) {
    return 1; // Mac Roman (treated as Latin-1 — correct for ASCII)
  }
  return 0;
}

} // namespace

SfntFontProgram::SfntFontProgram(std::string data) : m_data(std::move(data)) {
  const std::string tag = tag_at(m_data, 0);
  std::uint32_t sfnt_offset = 0;
  if (tag == "ttcf") {
    // TrueType Collection: read the first member's offset table.
    sfnt_offset = u32(m_data, 12);
  }
  parse_directory(sfnt_offset);
  parse_head();
  parse_maxp();
  parse_hhea();
  parse_hmtx();
  parse_cmap();
  parse_name();
}

bool SfntFontProgram::is_sfnt(std::string_view data) noexcept {
  if (data.size() < 4) {
    return false;
  }
  const std::string_view tag = data.substr(0, 4);
  return tag == std::string_view("\x00\x01\x00\x00", 4) || tag == "OTTO" ||
         tag == "true" || tag == "ttcf" || tag == "typ1";
}

void SfntFontProgram::parse_directory(std::uint32_t sfnt_offset) {
  const std::string version = tag_at(m_data, sfnt_offset);
  m_format =
      version == "OTTO" ? FontFormat::opentype_cff : FontFormat::truetype;

  const std::uint16_t num_tables = u16(m_data, sfnt_offset + 4);
  std::size_t record = sfnt_offset + 12;
  for (std::uint16_t i = 0; i < num_tables; ++i, record += 16) {
    m_tables.emplace(tag_at(m_data, record),
                     Table{u32(m_data, record + 8), u32(m_data, record + 12)});
  }
}

void SfntFontProgram::parse_head() {
  const auto head = table("head");
  if (!head) {
    return; // tolerate; keep the unitsPerEm default
  }
  m_units_per_em = u16(m_data, head->offset + 18);
  m_bbox = {s16(m_data, head->offset + 36), s16(m_data, head->offset + 38),
            s16(m_data, head->offset + 40), s16(m_data, head->offset + 42)};
}

void SfntFontProgram::parse_maxp() {
  const auto maxp = table("maxp");
  if (!maxp) {
    return;
  }
  m_glyph_count = u16(m_data, maxp->offset + 4);
}

void SfntFontProgram::parse_hhea() {
  const auto hhea = table("hhea");
  if (!hhea) {
    return;
  }
  m_number_of_h_metrics = u16(m_data, hhea->offset + 34);
}

void SfntFontProgram::parse_hmtx() {
  const auto hmtx = table("hmtx");
  if (!hmtx) {
    return;
  }
  m_advance_widths.reserve(m_number_of_h_metrics);
  for (std::uint16_t i = 0; i < m_number_of_h_metrics; ++i) {
    m_advance_widths.push_back(u16(m_data, hmtx->offset + i * 4U));
  }
}

void SfntFontProgram::parse_cmap() {
  const auto cmap = table("cmap");
  if (!cmap) {
    return;
  }
  const std::uint16_t count = u16(m_data, cmap->offset + 2);
  int best_score = 0;
  std::uint32_t best_offset = 0;
  for (std::uint16_t i = 0; i < count; ++i) {
    const std::size_t record = cmap->offset + 4 + i * 8U;
    const std::uint16_t platform = u16(m_data, record);
    const std::uint16_t encoding = u16(m_data, record + 2);
    if (platform == 3 && encoding == 0) {
      m_symbolic = true;
    }
    if (const int score = cmap_score(platform, encoding); score > best_score) {
      best_score = score;
      best_offset = cmap->offset + u32(m_data, record + 4);
    }
  }
  if (best_score > 0) {
    parse_cmap_subtable(best_offset);
  }
}

void SfntFontProgram::parse_cmap_subtable(std::uint32_t offset) {
  const auto map = [this](char32_t code, std::uint16_t glyph) {
    if (glyph == 0) {
      return;
    }
    m_cmap[code] = glyph;
    if (const auto it = m_reverse.find(glyph);
        it == m_reverse.end() || code < it->second) {
      m_reverse[glyph] = code;
    }
  };

  switch (const std::uint16_t format = u16(m_data, offset)) {
  case 0: { // byte encoding table
    for (std::uint32_t code = 0; code < 256; ++code) {
      map(code, u8(m_data, offset + 6 + code));
    }
    break;
  }
  case 4: { // segment mapping to delta values
    const std::size_t segs = u16(m_data, offset + 6) / 2U;
    const std::size_t end_codes = offset + 14;
    const std::size_t start_codes = end_codes + segs * 2 + 2;
    const std::size_t id_deltas = start_codes + segs * 2;
    const std::size_t id_range_offsets = id_deltas + segs * 2;
    for (std::size_t s = 0; s < segs; ++s) {
      const std::uint16_t end = u16(m_data, end_codes + s * 2);
      const std::uint16_t start = u16(m_data, start_codes + s * 2);
      const std::int16_t delta = s16(m_data, id_deltas + s * 2);
      const std::uint16_t range = u16(m_data, id_range_offsets + s * 2);
      for (std::uint32_t code = start; code <= end && code != 0xffff; ++code) {
        std::uint16_t glyph = 0;
        if (range == 0) {
          glyph = static_cast<std::uint16_t>(code + delta);
        } else {
          // idRangeOffset indexes into the glyphIdArray that follows.
          const std::size_t at = id_range_offsets + s * 2 + range +
                                 static_cast<std::size_t>(code - start) * 2;
          glyph = u16(m_data, at);
          if (glyph != 0) {
            glyph = static_cast<std::uint16_t>(glyph + delta);
          }
        }
        map(code, glyph);
      }
    }
    break;
  }
  case 6: { // trimmed table mapping
    const std::uint16_t first = u16(m_data, offset + 6);
    const std::uint16_t entries = u16(m_data, offset + 8);
    for (std::uint16_t i = 0; i < entries; ++i) {
      map(first + i, u16(m_data, offset + 10 + i * 2U));
    }
    break;
  }
  case 12: { // segmented coverage
    const std::uint32_t groups = u32(m_data, offset + 12);
    for (std::uint32_t g = 0; g < groups; ++g) {
      const std::size_t at = offset + 16 + g * 12U;
      const std::uint32_t start = u32(m_data, at);
      const std::uint32_t end = u32(m_data, at + 4);
      const std::uint32_t start_glyph = u32(m_data, at + 8);
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

void SfntFontProgram::parse_name() {
  const auto name = table("name");
  if (!name) {
    return;
  }
  const std::uint16_t count = u16(m_data, name->offset + 2);
  const std::uint16_t string_offset = u16(m_data, name->offset + 4);
  const std::size_t strings = name->offset + string_offset;

  int best_score = 0;
  for (std::uint16_t i = 0; i < count; ++i) {
    const std::size_t record = name->offset + 6 + i * 12U;
    const std::uint16_t platform = u16(m_data, record);
    const std::uint16_t name_id = u16(m_data, record + 6);
    if (name_id != 6 && name_id != 4) {
      continue;
    }
    // Prefer the PostScript name (6) over the full name (4), and a decodable
    // platform; higher score wins.
    const int score =
        (name_id == 6 ? 10 : 0) + (platform == 3 || platform == 0 ? 2 : 1);
    if (score <= best_score) {
      continue;
    }
    const std::uint16_t length = u16(m_data, record + 8);
    const std::uint16_t local_offset = u16(m_data, record + 10);
    m_name = (platform == 3 || platform == 0)
                 ? decode_utf16be(m_data, strings + local_offset, length)
                 : decode_latin1(m_data, strings + local_offset, length);
    best_score = score;
  }
}

FontFormat SfntFontProgram::format() const noexcept { return m_format; }

std::string SfntFontProgram::name() const { return m_name; }

std::uint16_t SfntFontProgram::glyph_count() const noexcept {
  return m_glyph_count;
}

std::uint16_t SfntFontProgram::units_per_em() const noexcept {
  return m_units_per_em;
}

bool SfntFontProgram::symbolic() const noexcept { return m_symbolic; }

FontBBox SfntFontProgram::bounding_box() const noexcept { return m_bbox; }

std::uint16_t SfntFontProgram::advance_width(std::uint16_t glyph) const {
  if (m_advance_widths.empty()) {
    return 0;
  }
  // Glyphs beyond `numberOfHMetrics` share the last advance (monospace tail).
  if (glyph >= m_advance_widths.size()) {
    return m_advance_widths.back();
  }
  return m_advance_widths[glyph];
}

std::uint16_t SfntFontProgram::glyph_for_code_point(char32_t code_point) const {
  const auto it = m_cmap.find(code_point);
  return it == m_cmap.end() ? 0 : it->second;
}

std::optional<char32_t>
SfntFontProgram::code_point_for_glyph(std::uint16_t glyph) const {
  const auto it = m_reverse.find(glyph);
  if (it == m_reverse.end()) {
    return std::nullopt;
  }
  return it->second;
}

const std::string &SfntFontProgram::data() const noexcept { return m_data; }

std::optional<SfntFontProgram::Table>
SfntFontProgram::table(std::string_view tag) const {
  const auto it = m_tables.find(tag);
  if (it == m_tables.end()) {
    return std::nullopt;
  }
  return it->second;
}

} // namespace odr::internal::font
