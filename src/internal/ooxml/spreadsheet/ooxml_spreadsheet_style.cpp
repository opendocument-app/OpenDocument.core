#include <internal/ooxml/spreadsheet/ooxml_spreadsheet_style.h>

namespace odr::internal::ooxml::spreadsheet {

Style::Style(pugi::xml_node node) : m_node{node} {}

StyleRegistry::StyleRegistry() = default;

StyleRegistry::StyleRegistry(pugi::xml_node styles_root) {
  generate_indices_(styles_root);
  generate_styles_(styles_root);
}

void StyleRegistry::generate_indices_(pugi::xml_node styles_root) {
  for (auto font : styles_root.child("fonts")) {
    m_fonts_index.push_back(font);
  }

  for (auto fill : styles_root.child("fills")) {
    m_fills_index.push_back(fill);
  }

  for (auto border : styles_root.child("borders")) {
    m_borders_index.push_back(border);
  }

  for (auto cell_master : styles_root.child("cellStyleXfs")) {
    m_cell_masters_index.push_back(cell_master);
  }

  for (auto cell_master : styles_root.child("cellStyleXfs")) {
    m_cell_masters_index.push_back(cell_master);
  }

  for (auto cell_format : styles_root.child("cellXfs")) {
    m_cell_formats_index.push_back(cell_format);
  }
}

void StyleRegistry::generate_styles_(pugi::xml_node styles_root) {}

} // namespace odr::internal::ooxml::spreadsheet
