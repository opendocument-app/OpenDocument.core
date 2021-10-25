#include <internal/ooxml/spreadsheet/ooxml_spreadsheet_style.h>

namespace odr::internal::ooxml::spreadsheet {

namespace {

const std::vector<Color> &color_index() {
  static const std::vector<Color> color_index{
      0x000000, 0xFFFFFF, 0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00, 0xFF00FF,
      0x00FFFF, 0x000000, 0xFFFFFF, 0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00,
      0xFF00FF, 0x00FFFF, 0x800000, 0x008000, 0x000080, 0x808000, 0x800080,
      0x008080, 0xC0C0C0, 0x808080, 0x9999FF, 0x993366, 0xFFFFCC, 0xCCFFFF,
      0x660066, 0xFF8080, 0x0066CC, 0xCCCCFF, 0x000080, 0xFF00FF, 0xFFFF00,
      0x00FFFF, 0x800080, 0x800000, 0x008080, 0x0000FF, 0x00CCFF, 0xCCFFFF,
      0xCCFFCC, 0xFFFF99, 0x99CCFF, 0xFF99CC, 0xCC99FF, 0xFFCC99, 0x3366FF,
      0x33CCCC, 0x99CC00, 0xFFCC00, 0xFF9900, 0xFF6600, 0x666699, 0x969696,
      0x003366, 0x339966, 0x003300, 0x333300, 0x993300, 0x993366, 0x333399,
      0x333333,
      /*system foreground, *system background*/
  };
  return color_index;
}

std::optional<TextAlign> read_horizontal(pugi::xml_attribute attribute) {
  std::string value = attribute.value();
  if (value == "center") {
    return TextAlign::center;
  }
  return {};
}

std::optional<VerticalAlign> read_vertical(pugi::xml_attribute attribute) {
  std::string value = attribute.value();
  if (value == "center") {
    return VerticalAlign::middle;
  }
  return {};
}

std::optional<Color> read_color(pugi::xml_node node) {
  if (auto indexed = node.attribute("indexed")) {
    return color_index().at(indexed.as_uint());
  }
  return {};
}

} // namespace

StyleRegistry::StyleRegistry() = default;

StyleRegistry::StyleRegistry(pugi::xml_node styles_root) {
  generate_indices_(styles_root);
}

common::ResolvedStyle StyleRegistry::cell_style(std::uint32_t i) const {
  common::ResolvedStyle result;

  auto cell_format = m_cell_formats_index.at(i);

  if (auto font_id = cell_format.attribute("fontId");
      cell_format.attribute("applyFont").as_bool() && font_id) {
    resolve_font_(font_id.as_uint(), result);
  }

  if (auto fill_id = cell_format.attribute("fillId");
      cell_format.attribute("applyFill").as_bool() && fill_id) {
    resolve_fill_(fill_id.as_uint(), result);
  }

  if (auto border_id = cell_format.attribute("borderId");
      cell_format.attribute("applyBorder").as_bool() && border_id) {
    resolve_fill_(border_id.as_uint(), result);
  }

  if (auto alignment = cell_format.child("alignment");
      cell_format.attribute("applyAlignment").as_bool() && alignment) {
    read_vertical(alignment.attribute("horizontal")); // TODO
    result.table_cell_style.vertical_align =
        read_vertical(alignment.attribute("vertical"));
  }

  if (auto protection = cell_format.child("protection");
      cell_format.attribute("applyProtection").as_bool() && protection) {
    // TODO
  }

  return result;
}

void StyleRegistry::resolve_font_(std::uint32_t i,
                                  common::ResolvedStyle &result) const {
  auto font = m_fonts_index.at(i);

  if (auto size = font.child("sz").attribute("val")) {
    result.text_style.font_size = Measure(size.as_float(), DynamicUnit("pt"));
  }
  if (auto font_name = font.child("name").attribute("val")) {
    result.text_style.font_name = font_name.value();
  }
  if (font.child("b")) {
    result.text_style.font_weight = FontWeight::bold;
  }
}

void StyleRegistry::resolve_fill_(std::uint32_t i,
                                  common::ResolvedStyle &result) const {
  // TODO
}

void StyleRegistry::resolve_border_(std::uint32_t i,
                                    common::ResolvedStyle &result) const {
  // TODO
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

  for (auto cell_format : styles_root.child("cellXfs")) {
    m_cell_formats_index.push_back(cell_format);
  }
}

} // namespace odr::internal::ooxml::spreadsheet
