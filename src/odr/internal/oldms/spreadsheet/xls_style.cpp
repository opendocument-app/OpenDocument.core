#include <odr/internal/oldms/spreadsheet/xls_style.hpp>

#include <odr/quantity.hpp>
#include <odr/style.hpp>

#include <array>
#include <optional>
#include <stdexcept>

namespace odr::internal::oldms::spreadsheet {
namespace {

/// The built-in color constants, icv 0x00-0x07 ([MS-XLS] 2.5.161).
constexpr std::array<std::uint32_t, 8> built_in_colors = {
    0x000000, 0xFFFFFF, 0xFF0000, 0x00FF00,
    0x0000FF, 0xFFFF00, 0xFF00FF, 0x00FFFF};

/// The default palette, icv 0x08-0x3F, used when no Palette record is present
/// ([MS-XLS] 2.5.161).
constexpr std::array<std::uint32_t, palette_color_count> default_palette = {
    0x000000, 0xFFFFFF, 0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00, 0xFF00FF,
    0x00FFFF, 0x800000, 0x008000, 0x000080, 0x808000, 0x800080, 0x008080,
    0xC0C0C0, 0x808080, 0x9999FF, 0x993366, 0xFFFFCC, 0xCCFFFF, 0x660066,
    0xFF8080, 0x0066CC, 0xCCCCFF, 0x000080, 0xFF00FF, 0xFFFF00, 0x00FFFF,
    0x800080, 0x800000, 0x008080, 0x0000FF, 0x00CCFF, 0xCCFFFF, 0xCCFFCC,
    0xFFFF99, 0x99CCFF, 0xFF99CC, 0xCC99FF, 0xFFCC99, 0x3366FF, 0x33CCCC,
    0x99CC00, 0xFFCC00, 0xFF9900, 0xFF6600, 0x666699, 0x969696, 0x003366,
    0x339966, 0x003300, 0x333300, 0x993300, 0x993366, 0x333399, 0x333333};

/// Resolves a color-table index ([MS-XLS] 2.5.161): built-in constants, then
/// the (custom or default) palette. The system/automatic values (0x40, 0x41,
/// 0x7FFF, ...) resolve to "unset".
std::optional<Color> icv_color(const std::uint16_t icv,
                               const std::span<const LongRgb> palette) {
  if (icv < built_in_colors.size()) {
    return Color(built_in_colors[icv]);
  }
  if (const std::size_t index = icv - built_in_colors.size();
      index < palette_color_count) {
    if (!palette.empty()) {
      const LongRgb &color = palette[index];
      return Color(color.red, color.green, color.blue);
    }
    return Color(default_palette[index]);
  }
  return std::nullopt;
}

/// Resolves a FontIndex ([MS-XLS] 2.5.129): values below 4 are zero-based,
/// values above 4 are one-based; 4 never occurs.
const StyleRegistry::Font &
font_at(const std::span<const StyleRegistry::Font> fonts,
        const std::uint16_t ifnt) {
  const std::size_t index = ifnt < 4 ? ifnt : ifnt - 1;
  if (ifnt == 4 || index >= fonts.size()) {
    throw std::runtime_error("xls: font index out of range");
  }
  return fonts[index];
}

} // namespace

StyleRegistry::StyleRegistry(std::vector<Font> fonts,
                             const std::span<const XfBody> xfs,
                             const std::span<const LongRgb> palette)
    : m_fonts{std::move(fonts)} {
  if (!palette.empty() && palette.size() != palette_color_count) {
    throw std::invalid_argument("StyleRegistry: unexpected palette size");
  }

  m_cell_styles.reserve(xfs.size());

  for (const XfBody &xf : xfs) {
    ResolvedStyle &style = m_cell_styles.emplace_back();

    const Font &font = font_at(m_fonts, xf.ifnt);
    TextStyle &text = style.text_style;
    text.font_name = font.name.c_str();
    if (font.fixed.dyHeight != 0) {
      text.font_size = Measure(font.fixed.dyHeight / 20.0, DynamicUnit("pt"));
    }
    text.font_weight =
        font.fixed.bls >= 600 ? FontWeight::bold : FontWeight::normal;
    text.font_style =
        font.fixed.fItalic != 0 ? FontStyle::italic : FontStyle::normal;
    text.font_underline = font.fixed.uls != 0;
    text.font_line_through = font.fixed.fStrikeOut != 0;
    text.font_color = icv_color(font.fixed.icv, palette);

    // For the solid pattern only icvFore is rendered; the other patterns are
    // approximated by their foreground color as well.
    if (xf.fls != 0) {
      style.table_cell_style.background_color = icv_color(xf.icvFore, palette);
    }
  }
}

const ResolvedStyle &StyleRegistry::cell_style(const std::uint16_t ixfe) const {
  if (ixfe >= m_cell_styles.size()) {
    throw std::out_of_range("StyleRegistry::cell_style: XF index out of range");
  }
  return m_cell_styles[ixfe];
}

} // namespace odr::internal::oldms::spreadsheet
