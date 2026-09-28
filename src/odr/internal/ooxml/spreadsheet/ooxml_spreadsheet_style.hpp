#pragma once

#include <odr/internal/common/style.hpp>

#include <optional>
#include <vector>

#include <pugixml.hpp>

namespace odr::internal::ooxml::spreadsheet {

class StyleRegistry final {
public:
  StyleRegistry();
  /// @p theme_root is the `a:theme` of the workbook, null where it has none.
  StyleRegistry(pugi::xml_node styles_root, pugi::xml_node theme_root);

  [[nodiscard]] ResolvedStyle cell_style(std::uint32_t i) const;

private:
  /// `lt1`, `dk1`, `lt2`, `dk2`, `accent1` to `accent6`, `hlink`, `folHlink`:
  /// the order a `theme` index counts in.
  std::vector<std::optional<Color>> m_theme_colors;
  std::vector<pugi::xml_node> m_fonts_index;
  std::vector<pugi::xml_node> m_borders_index;
  std::vector<pugi::xml_node> m_fills_index;
  std::vector<pugi::xml_node> m_cell_masters_index;
  std::vector<pugi::xml_node> m_cell_formats_index;

  void generate_indices_(pugi::xml_node styles_root);

  [[nodiscard]] std::optional<Color> read_color_(pugi::xml_node node) const;
  void resolve_font_(std::uint32_t i, ResolvedStyle &result) const;
  void resolve_border_(std::uint32_t i, ResolvedStyle &result) const;
  void resolve_fill_(std::uint32_t i, ResolvedStyle &result) const;
};

} // namespace odr::internal::ooxml::spreadsheet
