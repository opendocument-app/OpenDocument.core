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
  StyleRegistry(std::span<const Font> fonts, std::span<const XfBody> xfs,
                std::span<const LongRgb> palette);

  /// Font and cell style by XF index; throws for an unknown index.
  [[nodiscard]] const ResolvedStyle &cell_style(std::uint16_t ixfe) const;

private:
  std::vector<ResolvedStyle> m_cell_styles;
};

} // namespace odr::internal::oldms::spreadsheet
