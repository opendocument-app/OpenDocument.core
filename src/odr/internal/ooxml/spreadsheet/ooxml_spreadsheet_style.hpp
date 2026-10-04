#pragma once

#include <odr/internal/common/style.hpp>
#include <odr/internal/number_format/number_format.hpp>

#include <cstdint>

#include <optional>
#include <unordered_map>
#include <vector>

#include <pugixml.hpp>

namespace odr::internal::ooxml::spreadsheet {

class StyleRegistry final {
public:
  StyleRegistry();
  /// @p theme_root is the `a:theme` of the workbook, null where it has none.
  StyleRegistry(pugi::xml_node styles_root, pugi::xml_node theme_root);

  [[nodiscard]] ResolvedStyle cell_style(std::uint32_t i) const;
  /// The number format `cellXfs` index @p i names: a `numFmt`, else a
  /// built-in one of ECMA-376 18.8.30, else `General`.
  [[nodiscard]] const number_format::Format &
  number_format(std::uint32_t i) const;

  /// The `cellXfs` index of @p base with the delta applied. An equal `xf`,
  /// `font` or `fill` is reused, else one is appended.
  std::uint32_t create_cell_format(std::uint32_t base,
                                   const TableCellStyle &cell_style,
                                   const TextStyle &text_style);

private:
  pugi::xml_node m_styles_root;
  /// `lt1`, `dk1`, `lt2`, `dk2`, `accent1` to `accent6`, `hlink`, `folHlink`:
  /// the order a `theme` index counts in.
  std::vector<std::optional<Color>> m_theme_colors;
  std::vector<pugi::xml_node> m_fonts_index;
  std::vector<pugi::xml_node> m_borders_index;
  std::vector<pugi::xml_node> m_fills_index;
  std::vector<pugi::xml_node> m_cell_masters_index;
  std::vector<pugi::xml_node> m_cell_formats_index;
  std::unordered_map<std::uint32_t, number_format::Format> m_number_formats;

  void generate_indices_(pugi::xml_node styles_root);

  [[nodiscard]] std::optional<Color> read_color_(pugi::xml_node node) const;
  void resolve_font_(std::uint32_t i, ResolvedStyle &result) const;
  void resolve_border_(std::uint32_t i, ResolvedStyle &result) const;
  void resolve_fill_(std::uint32_t i, ResolvedStyle &result) const;
};

} // namespace odr::internal::ooxml::spreadsheet
