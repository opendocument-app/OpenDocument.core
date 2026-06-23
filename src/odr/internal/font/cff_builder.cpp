#include <odr/internal/font/cff_builder.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace odr::internal::font::cff {

namespace {

void put16(std::string &s, const std::uint16_t v) {
  s += static_cast<char>(v >> 8);
  s += static_cast<char>(v & 0xff);
}

/// A CFF DICT integer in the compact encoding (used for widths / bbox).
void dict_int(std::string &s, const int v) {
  if (v >= -107 && v <= 107) {
    s += static_cast<char>(v + 139);
  } else if (v >= 108 && v <= 1131) {
    const int u = v - 108;
    s += static_cast<char>((u >> 8) + 247);
    s += static_cast<char>(u & 0xff);
  } else if (v >= -1131 && v <= -108) {
    const int u = -v - 108;
    s += static_cast<char>((u >> 8) + 251);
    s += static_cast<char>(u & 0xff);
  } else if (v >= -32768 && v <= 32767) {
    s += static_cast<char>(28);
    put16(s, static_cast<std::uint16_t>(v));
  } else {
    s += static_cast<char>(29);
    s += static_cast<char>((v >> 24) & 0xff);
    s += static_cast<char>((v >> 16) & 0xff);
    s += static_cast<char>((v >> 8) & 0xff);
    s += static_cast<char>(v & 0xff);
  }
}

/// A CFF DICT integer in the fixed 5-byte form (`29 + int32`), so an operand
/// whose value (an offset) is not yet known can be sized before it is filled.
void dict_int_fixed(std::string &s, const std::int32_t v) {
  s += static_cast<char>(29);
  s += static_cast<char>((v >> 24) & 0xff);
  s += static_cast<char>((v >> 16) & 0xff);
  s += static_cast<char>((v >> 8) & 0xff);
  s += static_cast<char>(v & 0xff);
}

void dict_operator(std::string &s, const int op) {
  if (op >= 1200) {
    s += static_cast<char>(12);
    s += static_cast<char>(op - 1200);
  } else {
    s += static_cast<char>(op);
  }
}

/// Serialize a CFF INDEX from its members.
std::string build_index(const std::vector<std::string> &members) {
  std::string out;
  put16(out, static_cast<std::uint16_t>(members.size()));
  if (members.empty()) {
    return out; // count 0: no offSize/offsets
  }
  std::uint32_t total = 1;
  for (const std::string &m : members) {
    total += static_cast<std::uint32_t>(m.size());
  }
  const std::uint8_t off_size = total <= 0xff       ? 1
                                : total <= 0xffff   ? 2
                                : total <= 0xffffff ? 3
                                                    : 4;
  out += static_cast<char>(off_size);
  const auto put_off = [&](const std::uint32_t off) {
    for (int i = off_size - 1; i >= 0; --i) {
      out += static_cast<char>((off >> (8 * i)) & 0xff);
    }
  };
  std::uint32_t offset = 1;
  put_off(offset);
  for (const std::string &m : members) {
    offset += static_cast<std::uint32_t>(m.size());
    put_off(offset);
  }
  for (const std::string &m : members) {
    out += m;
  }
  return out;
}

} // namespace

std::string build_cff(const std::string_view name,
                      const std::vector<BuilderGlyph> &glyphs,
                      const double default_width, const double nominal_width,
                      const FontBBox bbox) {
  // CharStrings INDEX (one Type2 charstring per glyph).
  std::vector<std::string> charstrings;
  charstrings.reserve(glyphs.size());
  for (const BuilderGlyph &glyph : glyphs) {
    charstrings.push_back(glyph.charstring);
  }
  const std::string charstrings_index = build_index(charstrings);

  // String INDEX: every glyph name gets a custom SID (391 + position). Glyph 0
  // is the implicit `.notdef` (SID 0), so its name is not stored; the charset
  // lists SIDs for glyphs 1..n-1.
  std::vector<std::string> strings;
  for (std::size_t i = 1; i < glyphs.size(); ++i) {
    strings.push_back(glyphs[i].name);
  }
  const std::string string_index = build_index(strings);

  // Format-0 charset: SID per glyph 1..n-1.
  std::string charset;
  charset += static_cast<char>(0); // format 0
  for (std::size_t i = 1; i < glyphs.size(); ++i) {
    put16(charset, static_cast<std::uint16_t>(391 + (i - 1)));
  }

  // Private DICT: defaultWidthX (20), nominalWidthX (21).
  std::string private_dict;
  dict_int(private_dict, static_cast<int>(default_width));
  dict_operator(private_dict, 20);
  dict_int(private_dict, static_cast<int>(nominal_width));
  dict_operator(private_dict, 21);

  const std::string name_index =
      build_index({std::string(name.empty() ? "ODRType1" : name)});
  const std::string global_subrs = build_index({});

  // Top DICT, with the offsets to charset / CharStrings / Private filled once
  // the layout is known. Fixed-width offset integers keep the size constant.
  const auto top_dict = [&](const std::uint32_t charset_off,
                            const std::uint32_t charstrings_off,
                            const std::uint32_t private_off) {
    std::string d;
    dict_int(d, bbox.x_min);
    dict_int(d, bbox.y_min);
    dict_int(d, bbox.x_max);
    dict_int(d, bbox.y_max);
    dict_operator(d, 5); // FontBBox
    dict_int_fixed(d, static_cast<std::int32_t>(charset_off));
    dict_operator(d, 15); // charset
    dict_int_fixed(d, static_cast<std::int32_t>(charstrings_off));
    dict_operator(d, 17); // CharStrings
    dict_int_fixed(d, static_cast<std::int32_t>(private_dict.size()));
    dict_int_fixed(d, static_cast<std::int32_t>(private_off));
    dict_operator(d, 18); // Private [size offset]
    return d;
  };

  const std::string top_dict_probe = build_index({top_dict(0, 0, 0)});
  constexpr std::uint32_t header_size = 4;
  const auto prefix = static_cast<std::uint32_t>(
      header_size + name_index.size() + top_dict_probe.size() +
      string_index.size() + global_subrs.size());
  // Layout after the prefix: CharStrings, charset, Private.
  const std::uint32_t charstrings_off = prefix;
  const std::uint32_t charset_off =
      charstrings_off + static_cast<std::uint32_t>(charstrings_index.size());
  const std::uint32_t private_off =
      charset_off + static_cast<std::uint32_t>(charset.size());

  const std::string top_dict_index =
      build_index({top_dict(charset_off, charstrings_off, private_off)});

  std::string out;
  out += static_cast<char>(1); // major
  out += static_cast<char>(0); // minor
  out += static_cast<char>(4); // hdrSize
  out += static_cast<char>(4); // offSize (absolute offsets; legacy/unused)
  out += name_index;
  out += top_dict_index;
  out += string_index;
  out += global_subrs;
  out += charstrings_index;
  out += charset;
  out += private_dict;
  return out;
}

} // namespace odr::internal::font::cff
