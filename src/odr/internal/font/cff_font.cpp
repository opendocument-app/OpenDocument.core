#include <odr/internal/font/cff_font.hpp>

#include <odr/internal/font/cff_standard_strings.hpp>
#include <odr/internal/pdf/pdf_encoding.hpp>
#include <odr/internal/util/byte_string.hpp>
#include <odr/internal/util/stream_util.hpp>

#include <cmath>
#include <cstdint>
#include <istream>
#include <iterator>
#include <map>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace odr::internal::font::cff {

namespace {

/// Two-byte operators (`escape b`) are folded into the one-byte keyspace as
/// `escape_op_base + b` so a single map keys every operator.
constexpr std::uint16_t escape_op_base = 1200;

/// CFF Top/Private DICT operators we care about. Two-byte (escape `12 b`)
/// operators are encoded as `escape_op_base + b` so they share one keyspace.
enum Operator : std::uint16_t {
  op_charset = 15,
  op_encoding = 16,
  op_char_strings = 17,
  op_private = 18,
  op_default_width_x = 20,
  op_nominal_width_x = 21,
  op_font_matrix = 1207,
  op_font_bbox = 5,
  op_ros = 1230,
  op_charstring_type = 1206,
};

/// Number-operand byte markers shared by DICT data and Type2 charstrings
/// (Adobe TN #5176 Table 3 / TN #5177). Bytes 32..254 encode small integers
/// directly; these are the out-of-band markers.
enum NumberMarker : std::uint8_t {
  marker_escape = 12,   // two-byte operator escape (DICT/charstring)
  marker_shortint = 28, // big-endian int16 follows
  marker_longint = 29,  // big-endian int32 follows (DICT only)
  marker_real = 30,     // packed-BCD real follows (DICT only)
  marker_int_min = 32,  // first byte of the direct small-integer range
  marker_fixed = 255,   // 16.16 fixed follows (charstring only)
};

/// BCD nibble codes for a DICT real number (Adobe TN #5176 Table 5).
enum RealNibble : std::uint8_t {
  nibble_dot = 0x0a,
  nibble_exp = 0x0b,
  nibble_exp_neg = 0x0c,
  nibble_minus = 0x0e,
  nibble_end = 0x0f,
};

/// Type2 charstring operators relevant to leading-width detection (Adobe
/// TN #5177).
enum CharstringOp : std::uint8_t {
  cs_hstem = 1,
  cs_vstem = 3,
  cs_vmoveto = 4,
  cs_endchar = 14,
  cs_hstemhm = 18,
  cs_hintmask = 19,
  cs_cntrmask = 20,
  cs_rmoveto = 21,
  cs_hmoveto = 22,
  cs_vstemhm = 23,
};

namespace bs = util::byte_string;

/// CFF is offset-addressed, so these adapt `byte_string`'s front-relative reads
/// to an absolute offset @p p into the buffer. An out-of-range @p p yields an
/// empty view, which `byte_string` reports as a (throwing) short read.
[[nodiscard]] std::string_view at(const std::string_view d,
                                  const std::uint32_t p) {
  return p <= d.size() ? d.substr(p) : std::string_view{};
}

[[nodiscard]] std::uint8_t u8(const std::string_view d, const std::uint32_t p) {
  return bs::read_u8(at(d, p));
}

/// Read a big-endian unsigned of @p size bytes (1..4) at @p p.
[[nodiscard]] std::uint32_t read_be(const std::string_view d,
                                    const std::uint32_t p,
                                    const std::uint32_t size) {
  return bs::read_uint_be(at(d, p), size);
}

/// A parsed DICT: operator -> operands. Reals are decoded to double; integers
/// stay exact within double range (CFF integers fit).
using Dict = std::map<std::uint16_t, std::vector<double>>;

/// Parse a CFF DICT occupying the byte range [begin, end) of @p d.
[[nodiscard]] Dict parse_dict(const std::string_view d,
                              const std::uint32_t begin,
                              const std::uint32_t end) {
  Dict dict;
  std::vector<double> operands;
  std::uint32_t p = begin;
  while (p < end) {
    const std::uint8_t b0 = u8(d, p);
    if (b0 <= 21) {
      // operator
      std::uint16_t op = b0;
      ++p;
      if (b0 == marker_escape) {
        op = escape_op_base + u8(d, p);
        ++p;
      }
      dict[op] = operands;
      operands.clear();
    } else if (b0 == marker_shortint) {
      operands.push_back(static_cast<std::int16_t>(read_be(d, p + 1, 2)));
      p += 3;
    } else if (b0 == marker_longint) {
      operands.push_back(static_cast<std::int32_t>(read_be(d, p + 1, 4)));
      p += 5;
    } else if (b0 == marker_real) {
      // real number: packed BCD nibbles, terminated by nibble_end.
      ++p;
      std::string number;
      bool done = false;
      while (!done && p < end) {
        const std::uint8_t byte = u8(d, p++);
        for (const std::uint8_t nibble :
             {static_cast<std::uint8_t>(byte >> 4),
              static_cast<std::uint8_t>(byte & 0x0f)}) {
          if (nibble <= 9) {
            number += static_cast<char>('0' + nibble);
          } else if (nibble == nibble_dot) {
            number += '.';
          } else if (nibble == nibble_exp) {
            number += 'E';
          } else if (nibble == nibble_exp_neg) {
            number += "E-";
          } else if (nibble == nibble_minus) {
            number += '-';
          } else if (nibble == nibble_end) {
            done = true;
            break;
          }
        }
      }
      try {
        operands.push_back(number.empty() ? 0.0 : std::stod(number));
      } catch (const std::exception &) {
        operands.push_back(0.0);
      }
    } else if (b0 >= 32 && b0 <= 246) {
      operands.push_back(static_cast<int>(b0) - 139);
      ++p;
    } else if (b0 >= 247 && b0 <= 250) {
      operands.push_back((static_cast<int>(b0) - 247) * 256 + u8(d, p + 1) +
                         108);
      p += 2;
    } else if (b0 >= 251 && b0 <= 254) {
      operands.push_back(-(static_cast<int>(b0) - 251) * 256 - u8(d, p + 1) -
                         108);
      p += 2;
    } else {
      throw std::runtime_error("cff: invalid DICT byte");
    }
  }
  return dict;
}

/// Predefined charset ids (Adobe TN #5176 §13). A Top DICT `/charset` of 0/1/2
/// (or an omitted `/charset`, defaulting to 0) selects one of these instead of
/// a charset offset.
enum PredefinedCharset : std::uint32_t {
  predefined_charset_iso_adobe = 0,
  predefined_charset_expert = 1,
  predefined_charset_expert_subset = 2,
};

/// Predefined Expert charset: glyph -> SID (Adobe TN #5176 Appendix C). The
/// ISOAdobe charset is the identity (SID == GID) so it needs no table.
constexpr std::uint16_t expert_charset[] = {
    0,   1,   229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 13,  14,
    15,  99,  239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 27,  28,
    249, 250, 251, 252, 253, 254, 255, 256, 257, 258, 259, 260, 261, 262,
    263, 264, 265, 266, 109, 110, 267, 268, 269, 270, 271, 272, 273, 274,
    275, 276, 277, 278, 279, 280, 281, 282, 283, 284, 285, 286, 287, 288,
    289, 290, 291, 292, 293, 294, 295, 296, 297, 298, 299, 300, 301, 302,
    303, 304, 305, 306, 307, 308, 309, 310, 311, 312, 313, 314, 315, 316,
    317, 318, 158, 155, 163, 319, 320, 321, 322, 323, 324, 325, 326, 150,
    164, 169, 327, 328, 329, 330, 331, 332, 333, 334, 335, 336, 337, 338,
    339, 340, 341, 342, 343, 344, 345, 346, 347, 348, 349, 350, 351, 352,
    353, 354, 355, 356, 357, 358, 359, 360, 361, 362, 363, 364, 365, 366,
    367, 368, 369, 370, 371, 372, 373, 374, 375, 376, 377, 378};

/// Predefined ExpertSubset charset: glyph -> SID (Adobe TN #5176 Appendix C).
constexpr std::uint16_t expert_subset_charset[] = {
    0,   1,   231, 232, 235, 236, 237, 238, 13,  14,  15,  99,  239, 240, 241,
    242, 243, 244, 245, 246, 247, 248, 27,  28,  249, 250, 251, 253, 254, 255,
    256, 257, 258, 259, 260, 261, 262, 263, 264, 265, 266, 109, 110, 267, 268,
    269, 270, 272, 300, 301, 302, 305, 314, 315, 158, 155, 163, 320, 321, 322,
    323, 324, 325, 326, 150, 164, 169, 327, 328, 329, 330, 331, 332, 333, 334,
    335, 336, 337, 338, 339, 340, 341, 342, 343, 344, 345, 346};

} // namespace

bool CffFont::is_cff(const std::string_view data) {
  // major version 1, header size >= 4, offSize in 1..4.
  return data.size() >= 4 && static_cast<std::uint8_t>(data[0]) == 1 &&
         static_cast<std::uint8_t>(data[2]) >= 4 &&
         static_cast<std::uint8_t>(data[3]) >= 1 &&
         static_cast<std::uint8_t>(data[3]) <= 4;
}

CffFont::CffFont(std::unique_ptr<std::istream> stream) {
  if (stream == nullptr) {
    throw std::invalid_argument("cff: null input stream");
  }
  m_data = util::stream::read(*stream);
  parse();
}

CffFont::CffFont(std::string data) : m_data{std::move(data)} { parse(); }

std::vector<CffFont::Range> CffFont::read_index(const std::uint32_t offset,
                                                std::uint32_t &end) const {
  const std::string_view d{m_data};
  const std::uint16_t count = static_cast<std::uint16_t>(read_be(d, offset, 2));
  if (count == 0) {
    end = offset + 2;
    return {};
  }
  const std::uint8_t off_size = u8(d, offset + 2);
  if (off_size < 1 || off_size > 4) {
    throw std::runtime_error("cff: bad INDEX offSize");
  }
  const std::uint32_t offset_array = offset + 3;
  const std::uint32_t data_base = offset_array + (count + 1) * off_size - 1;

  std::vector<Range> members;
  members.reserve(count);
  std::uint32_t prev = read_be(d, offset_array, off_size);
  for (std::uint16_t i = 1; i <= count; ++i) {
    const std::uint32_t next =
        read_be(d, offset_array + i * off_size, off_size);
    members.push_back({data_base + prev, next - prev});
    prev = next;
  }
  end = data_base + prev;
  return members;
}

void CffFont::parse() {
  const std::string_view d{m_data};
  if (!is_cff(d)) {
    throw std::runtime_error("cff: not a CFF font");
  }
  const std::uint8_t header_size = u8(d, 2);

  std::uint32_t pos = header_size;
  std::uint32_t end = 0;

  // Name INDEX (one entry per font; we read the first as the font name).
  const std::vector<Range> names = read_index(pos, end);
  if (!names.empty()) {
    m_name = std::string(d.substr(names[0].offset, names[0].length));
  }
  pos = end;

  // Top DICT INDEX (one entry — the font's Top DICT).
  const std::vector<Range> top_dicts = read_index(pos, end);
  if (top_dicts.empty()) {
    throw std::runtime_error("cff: empty Top DICT INDEX");
  }
  pos = end;

  // String INDEX (custom strings, SID >= 391).
  m_strings = read_index(pos, end);
  pos = end;

  // Global Subr INDEX follows; not needed for the facts (skipped).

  parse_top_dict(top_dicts[0]);
}

void CffFont::parse_top_dict(const Range top_dict) {
  const std::string_view d{m_data};
  const Dict dict =
      parse_dict(d, top_dict.offset, top_dict.offset + top_dict.length);

  m_cid_keyed = dict.find(op_ros) != dict.end();

  if (const auto it = dict.find(op_font_matrix);
      it != dict.end() && it->second.size() == 6 && it->second[0] != 0.0) {
    // unitsPerEm = 1 / FontMatrix[0] (design units per em).
    const double upm = 1.0 / it->second[0];
    if (upm > 0 && upm < 65536) {
      m_units_per_em = static_cast<std::uint16_t>(std::lround(upm));
    }
  }

  if (const auto it = dict.find(op_font_bbox);
      it != dict.end() && it->second.size() == 4) {
    m_bbox = {static_cast<std::int16_t>(it->second[0]),
              static_cast<std::int16_t>(it->second[1]),
              static_cast<std::int16_t>(it->second[2]),
              static_cast<std::int16_t>(it->second[3])};
  }

  if (const auto it = dict.find(op_char_strings); it != dict.end()) {
    std::uint32_t end = 0;
    m_charstrings =
        read_index(static_cast<std::uint32_t>(it->second.at(0)), end);
  }

  if (const auto it = dict.find(op_private);
      it != dict.end() && it->second.size() == 2) {
    const auto size = static_cast<std::uint32_t>(it->second[0]);
    const auto offset = static_cast<std::uint32_t>(it->second[1]);
    parse_private_dict({offset, size});
  }

  // charset: an offset past the predefined ids (0/1/2) is a custom charset;
  // 0/1/2 (or an omitted `/charset`, defaulting to 0 = ISOAdobe) select a
  // predefined charset. CID-keyed fonts always carry a custom (CID) charset, so
  // the predefined name charsets only apply to name-keyed fonts.
  std::uint32_t charset = predefined_charset_iso_adobe;
  if (const auto it = dict.find(op_charset); it != dict.end()) {
    charset = static_cast<std::uint32_t>(it->second.at(0));
  }
  if (charset > predefined_charset_expert_subset) {
    parse_charset(charset);
  } else if (!m_cid_keyed) {
    load_predefined_charset(charset);
  }
}

void CffFont::load_predefined_charset(const std::uint32_t id) {
  const std::uint16_t glyphs = glyph_count();
  m_charset.assign(glyphs, 0); // glyph 0 (.notdef) -> SID 0
  if (id == predefined_charset_iso_adobe) {
    // ISOAdobe is the identity: glyph i carries SID i (Adobe TN #5176 App. C).
    for (std::uint16_t gid = 1; gid < glyphs; ++gid) {
      m_charset[gid] = gid;
    }
    return;
  }
  const std::uint16_t *table = nullptr;
  std::size_t size = 0;
  if (id == predefined_charset_expert) {
    table = expert_charset;
    size = std::size(expert_charset);
  } else { // predefined_charset_expert_subset
    table = expert_subset_charset;
    size = std::size(expert_subset_charset);
  }
  for (std::uint16_t gid = 1; gid < glyphs && gid < size; ++gid) {
    m_charset[gid] = table[gid];
  }
}

void CffFont::parse_private_dict(const Range private_dict) {
  if (private_dict.length == 0) {
    return;
  }
  const std::string_view d{m_data};
  const Dict dict = parse_dict(d, private_dict.offset,
                               private_dict.offset + private_dict.length);
  if (const auto it = dict.find(op_default_width_x); it != dict.end()) {
    m_default_width = it->second.at(0);
  }
  if (const auto it = dict.find(op_nominal_width_x); it != dict.end()) {
    m_nominal_width = it->second.at(0);
  }
}

void CffFont::parse_charset(const std::uint32_t offset) {
  const std::string_view d{m_data};
  const std::uint16_t glyphs = glyph_count();
  m_charset.assign(glyphs, 0); // glyph 0 (.notdef) -> SID/CID 0

  const std::uint8_t format = u8(d, offset);
  std::uint32_t p = offset + 1;
  std::uint16_t gid = 1; // glyph 0 is implicit
  if (format == 0) {
    for (; gid < glyphs; ++gid) {
      m_charset[gid] = static_cast<std::uint16_t>(read_be(d, p, 2));
      p += 2;
    }
  } else if (format == 1 || format == 2) {
    const std::uint32_t n_left_size = (format == 1) ? 1 : 2;
    while (gid < glyphs) {
      const auto first = static_cast<std::uint16_t>(read_be(d, p, 2));
      p += 2;
      const std::uint32_t n_left = read_be(d, p, n_left_size);
      p += n_left_size;
      for (std::uint32_t i = 0; i <= n_left && gid < glyphs; ++i, ++gid) {
        m_charset[gid] = static_cast<std::uint16_t>(first + i);
      }
    }
  } else {
    throw std::runtime_error("cff: unknown charset format");
  }
}

std::string CffFont::string_for_sid(const std::uint16_t sid) const {
  // SID 0..390 index the generated CFF standard strings; 391+ index the font's
  // String INDEX.
  if (sid < cff_standard_strings_size) {
    return std::string(cff_standard_strings[sid]);
  }
  const auto index =
      static_cast<std::uint16_t>(sid - cff_standard_strings_size);
  if (index >= m_strings.size()) {
    return {};
  }
  const std::string_view d{m_data};
  return std::string(
      d.substr(m_strings[index].offset, m_strings[index].length));
}

std::optional<std::int32_t>
CffFont::charstring_width(const std::uint16_t glyph) const {
  if (glyph >= m_charstrings.size()) {
    return std::nullopt;
  }
  const std::string_view d{m_data};
  const Range cs = m_charstrings[glyph];
  const std::uint32_t end = cs.offset + cs.length;
  std::uint32_t p = cs.offset;

  // Walk operands until the first width-bearing operator; the Type2 width, when
  // present, is the first operand and makes the operand count exceed the
  // operator's nominal arity (ISO/Adobe Type2 charstring spec, "width").
  std::int32_t count = 0;
  double first = 0.0;
  while (p < end) {
    const std::uint8_t b0 = u8(d, p);
    if (b0 >= marker_int_min || b0 == marker_shortint) {
      // operand
      double value = 0.0;
      if (b0 == marker_shortint) {
        value = static_cast<std::int16_t>(read_be(d, p + 1, 2));
        p += 3;
      } else if (b0 <= 246) {
        value = static_cast<int>(b0) - 139;
        p += 1;
      } else if (b0 <= 250) {
        value = (static_cast<int>(b0) - 247) * 256 + u8(d, p + 1) + 108;
        p += 2;
      } else if (b0 <= 254) {
        value = -(static_cast<int>(b0) - 251) * 256 - u8(d, p + 1) - 108;
        p += 2;
      } else { // marker_fixed: 16.16 fixed
        value = static_cast<std::int32_t>(read_be(d, p + 1, 4)) / 65536.0;
        p += 5;
      }
      if (count == 0) {
        first = value;
      }
      ++count;
    } else {
      // operator
      std::uint16_t op = b0;
      if (b0 == marker_escape) {
        op = escape_op_base + u8(d, p + 1);
      }
      bool width_possible = false;
      switch (op) {
      case cs_hstem:
      case cs_vstem:
      case cs_hstemhm:
      case cs_vstemhm:
      case cs_hintmask:
      case cs_cntrmask:
        width_possible = (count % 2) == 1;
        break;
      case cs_rmoveto: // 2 args
        width_possible = count > 2;
        break;
      case cs_hmoveto: // 1 arg
      case cs_vmoveto: // 1 arg
        width_possible = count > 1;
        break;
      case cs_endchar: // 0 args, or 4 for seac
        width_possible = (count % 2) == 1;
        break;
      default:
        // Any other operator before a width-bearing one: no explicit width.
        return std::nullopt;
      }
      return width_possible
                 ? std::optional<std::int32_t>(static_cast<std::int32_t>(first))
                 : std::nullopt;
    }
  }
  return std::nullopt;
}

FontFormat CffFont::format() const noexcept { return FontFormat::cff; }

std::string CffFont::name() const { return m_name; }

std::uint16_t CffFont::glyph_count() const noexcept {
  return static_cast<std::uint16_t>(m_charstrings.size());
}

std::uint16_t CffFont::units_per_em() const noexcept { return m_units_per_em; }

bool CffFont::symbolic() const noexcept {
  // A CID-keyed CFF has no name-based encoding; treat it as symbolic. A
  // name-keyed CFF is reported non-symbolic (its charset gives glyph names).
  return m_cid_keyed;
}

FontBBox CffFont::bounding_box() const noexcept { return m_bbox; }

std::uint16_t CffFont::advance_width(const std::uint16_t glyph) const {
  if (const std::optional<std::int32_t> width = charstring_width(glyph);
      width.has_value()) {
    return static_cast<std::uint16_t>(m_nominal_width + *width);
  }
  return static_cast<std::uint16_t>(m_default_width);
}

std::uint16_t CffFont::glyph_for_code_point(const char32_t code_point) const {
  // Reverse of code_point_for_glyph over the resolvable (custom) charset.
  for (std::uint16_t glyph = 1; glyph < glyph_count(); ++glyph) {
    if (code_point_for_glyph(glyph) == code_point) {
      return glyph;
    }
  }
  return 0;
}

std::optional<char32_t>
CffFont::code_point_for_glyph(const std::uint16_t glyph) const {
  const std::string name = glyph_name(glyph);
  if (name.empty()) {
    return std::nullopt;
  }
  // Glyph name -> Unicode via the Adobe Glyph List (and the algorithmic
  // `uniXXXX`/`uXXXXXX` forms), reusing the pdf module's AGL. The first code
  // point of a (possibly multi-character) mapping is returned; surrogate pairs
  // are recombined.
  const std::u16string utf16 = pdf::glyph_name_to_unicode(name);
  if (utf16.empty()) {
    return std::nullopt;
  }
  char32_t code_point = utf16[0];
  if (code_point >= 0xd800 && code_point <= 0xdbff && utf16.size() >= 2) {
    code_point = 0x10000 + ((code_point - 0xd800) << 10) + (utf16[1] - 0xdc00);
  }
  return code_point;
}

bool CffFont::is_cid_keyed() const noexcept { return m_cid_keyed; }

std::string CffFont::glyph_name(const std::uint16_t glyph) const {
  if (m_cid_keyed || glyph >= m_charset.size()) {
    return {};
  }
  return string_for_sid(m_charset[glyph]);
}

std::uint16_t CffFont::cid_for_glyph(const std::uint16_t glyph) const {
  if (!m_cid_keyed || glyph >= m_charset.size()) {
    return 0;
  }
  return m_charset[glyph];
}

std::uint16_t CffFont::glyph_for_cid(const std::uint16_t cid) const {
  if (!m_cid_keyed) {
    return cid; // identity when not CID-keyed
  }
  for (std::size_t glyph = 0; glyph < m_charset.size(); ++glyph) {
    if (m_charset[glyph] == cid) {
      return static_cast<std::uint16_t>(glyph);
    }
  }
  return 0;
}

std::string_view CffFont::data() const noexcept { return m_data; }

} // namespace odr::internal::font::cff
