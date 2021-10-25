#ifndef ODR_INTERNAL_OOXML_SPREADSHEET_STYLE_H
#define ODR_INTERNAL_OOXML_SPREADSHEET_STYLE_H

#include <pugixml.hpp>
#include <vector>

namespace odr::internal::ooxml::spreadsheet {

class Style final {
public:
  explicit Style(pugi::xml_node node);

private:
  pugi::xml_node m_node;
};

class StyleRegistry final {
public:
  StyleRegistry();
  explicit StyleRegistry(pugi::xml_node styles_root);

private:
  std::vector<pugi::xml_node> m_fonts_index;
  std::vector<pugi::xml_node> m_borders_index;
  std::vector<pugi::xml_node> m_fills_index;
  std::vector<pugi::xml_node> m_cell_masters_index;
  std::vector<pugi::xml_node> m_cell_formats_index;

  void generate_indices_(pugi::xml_node styles_root);
  void generate_styles_(pugi::xml_node styles_root);
};

} // namespace odr::internal::ooxml::spreadsheet

#endif // ODR_INTERNAL_OOXML_SPREADSHEET_STYLE_H
