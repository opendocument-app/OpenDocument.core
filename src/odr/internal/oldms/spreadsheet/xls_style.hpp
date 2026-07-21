#pragma once

#include <odr/internal/common/style.hpp>
#include <odr/internal/oldms/spreadsheet/xls_structs.hpp>

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace odr::internal::oldms::spreadsheet {

/// Resolves the workbook-global style records (Font, XF, Palette) into one
/// display style per XF record, indexed by a cell's ixfe.
class StyleRegistry final {
public:
  /// A Font record ([MS-XLS] 2.4.122), in file order.
  struct Font final {
    FontFixed fixed;
    std::string name;
  };

  StyleRegistry() = default;
  /// `palette` is the Palette record's `palette_color_count` colors, or empty
  /// when the record is absent (the spec's default palette applies).
  StyleRegistry(std::vector<Font> fonts, std::span<const XfBody> xfs,
                std::span<const LongRgb> palette);

  /// The resolved style of an XF record, by XF index (a cell's ixfe): the
  /// text (font) side and the cell (fill) side. Throws if the index has no
  /// XF record.
  [[nodiscard]] const ResolvedStyle &cell_style(std::uint16_t ixfe) const;

private:
  /// Owns the font names: `TextStyle::font_name` (`const char *`) points into
  /// them. Never modified after construction (moving the registry is fine —
  /// the strings themselves do not move).
  std::vector<Font> m_fonts;
  std::vector<ResolvedStyle> m_cell_styles;
};

} // namespace odr::internal::oldms::spreadsheet
