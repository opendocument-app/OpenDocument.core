#include <odr/internal/pdf/pdf_afm.hpp>

#include <odr/internal/pdf/pdf_afm_data.hpp>

#include <algorithm>
#include <array>
#include <cctype>

namespace odr::internal::pdf {

namespace {

const afm_data::FontMetrics &metrics_of(const StandardFont font) {
  return afm_data::fonts[static_cast<std::size_t>(font)];
}

/// ISO 32000-1 Table 121 `/FontDescriptor` `/Flags` bits used here.
constexpr std::uint32_t flag_fixed_pitch = 1U << 0;
constexpr std::uint32_t flag_serif = 1U << 1;
constexpr std::uint32_t flag_italic = 1U << 6;
constexpr std::uint32_t flag_force_bold = 1U << 18;

/// The `/BaseFont` name with a subset prefix (`ABCDEF+`) stripped, lowercased
/// for case-insensitive substring matching.
std::string normalize_name(std::string_view base_font) {
  if (base_font.size() > 7 && base_font[6] == '+') {
    bool prefix = true;
    for (std::size_t i = 0; i < 6; ++i) {
      if (base_font[i] < 'A' || base_font[i] > 'Z') {
        prefix = false;
        break;
      }
    }
    if (prefix) {
      base_font.remove_prefix(7);
    }
  }
  std::string result(base_font);
  for (char &c : result) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return result;
}

bool contains(const std::string &haystack, std::string_view needle) {
  return haystack.find(needle) != std::string::npos;
}

enum class Family { sans, serif, mono, symbol, zapf };

Family classify_family(const std::string &name, const std::uint32_t flags) {
  if (contains(name, "zapfdingbats") || contains(name, "dingbats")) {
    return Family::zapf;
  }
  if (contains(name, "symbol")) {
    return Family::symbol;
  }
  if (contains(name, "courier") || contains(name, "mono") ||
      contains(name, "consol")) {
    return Family::mono;
  }
  if (contains(name, "times") || contains(name, "georgia") ||
      contains(name, "roman") || contains(name, "serif") ||
      contains(name, "minion") || contains(name, "garamond")) {
    return Family::serif;
  }
  if (contains(name, "arial") || contains(name, "helvetica") ||
      contains(name, "verdana") || contains(name, "tahoma") ||
      contains(name, "segoe") || contains(name, "sans")) {
    return Family::sans;
  }
  // No name hint: fall back to the descriptor flags.
  if ((flags & flag_fixed_pitch) != 0) {
    return Family::mono;
  }
  if ((flags & flag_serif) != 0) {
    return Family::serif;
  }
  return Family::sans;
}

StandardFont pick_metrics(const Family family, const bool bold,
                          const bool italic) {
  switch (family) {
  case Family::serif:
    if (bold && italic) {
      return StandardFont::times_bold_italic;
    }
    if (bold) {
      return StandardFont::times_bold;
    }
    if (italic) {
      return StandardFont::times_italic;
    }
    return StandardFont::times_roman;
  case Family::mono:
    if (bold && italic) {
      return StandardFont::courier_bold_oblique;
    }
    if (bold) {
      return StandardFont::courier_bold;
    }
    if (italic) {
      return StandardFont::courier_oblique;
    }
    return StandardFont::courier;
  case Family::symbol:
    return StandardFont::symbol;
  case Family::zapf:
    return StandardFont::zapf_dingbats;
  case Family::sans:
  default:
    if (bold && italic) {
      return StandardFont::helvetica_bold_oblique;
    }
    if (bold) {
      return StandardFont::helvetica_bold;
    }
    if (italic) {
      return StandardFont::helvetica_oblique;
    }
    return StandardFont::helvetica;
  }
}

std::string_view family_stack(const Family family) {
  switch (family) {
  case Family::serif:
    return "'Times New Roman',Times,serif";
  case Family::mono:
    return "'Courier New',Courier,monospace";
  case Family::symbol:
    return "Symbol,serif";
  case Family::zapf:
    return "'Zapf Dingbats',fantasy";
  case Family::sans:
  default:
    return "Helvetica,Arial,sans-serif";
  }
}

} // namespace

FontSubstitute resolve_font_substitute(const std::string_view base_font,
                                       const std::uint32_t flags,
                                       const int font_weight,
                                       const double italic_angle) {
  const std::string name = normalize_name(base_font);
  const Family family = classify_family(name, flags);

  const bool bold = contains(name, "bold") || font_weight >= 600 ||
                    (flags & flag_force_bold) != 0;
  // Symbol and ZapfDingbats have no bold/italic AFM variant; the name may still
  // carry the words, but there is nothing to select.
  const bool italic = family != Family::symbol && family != Family::zapf &&
                      (contains(name, "italic") || contains(name, "oblique") ||
                       (flags & flag_italic) != 0 || italic_angle != 0);

  FontSubstitute substitute;
  substitute.css_family = std::string(family_stack(family));
  substitute.bold = bold;
  substitute.italic = italic;
  substitute.metrics = pick_metrics(family, bold, italic);
  return substitute;
}

std::optional<double> afm_width(const StandardFont font,
                                const std::string_view glyph_name) {
  const afm_data::FontMetrics &m = metrics_of(font);
  const afm_data::GlyphWidth *const begin = m.glyphs;
  const afm_data::GlyphWidth *const end = m.glyphs + m.glyph_count;
  const auto *const it = std::lower_bound(
      begin, end, glyph_name,
      [](const afm_data::GlyphWidth &g, const std::string_view name) {
        return g.name < name;
      });
  if (it != end && it->name == glyph_name) {
    return it->width;
  }
  return std::nullopt;
}

std::optional<double> afm_code_width(const StandardFont font,
                                     const std::uint8_t code) {
  const std::int16_t width = metrics_of(font).code_widths[code];
  if (width < 0) {
    return std::nullopt;
  }
  return width;
}

double afm_ascender(const StandardFont font) {
  return metrics_of(font).ascender / 1000.0;
}

} // namespace odr::internal::pdf
