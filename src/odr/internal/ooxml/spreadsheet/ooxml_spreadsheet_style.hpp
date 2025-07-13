#pragma once

#include <odr/internal/common/style.hpp>

#include <memory>
#include <unordered_map>
#include <vector>

#include <pugixml.hpp>

namespace odr::internal::ooxml::spreadsheet {

class StyleRegistry final {
public:
  StyleRegistry();
  explicit StyleRegistry(pugi::xml_node styles_root);

  [[nodiscard]] ResolvedStyle cell_style(std::uint32_t i) const;

private:
  std::vector<pugi::xml_node> m_fonts_index;
  std::vector<pugi::xml_node> m_borders_index;
  std::vector<pugi::xml_node> m_fills_index;
  std::vector<pugi::xml_node> m_cell_masters_index;
  std::vector<pugi::xml_node> m_cell_formats_index;

  void generate_indices_(pugi::xml_node styles_root);

  void resolve_font_(std::uint32_t i, ResolvedStyle &result) const;
  void resolve_border_(std::uint32_t i, ResolvedStyle &result) const;
  void resolve_fill_(std::uint32_t i, ResolvedStyle &result) const;
};

} // namespace odr::internal::ooxml::spreadsheet
