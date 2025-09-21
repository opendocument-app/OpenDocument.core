#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_style.hpp>

#include <odr/internal/html/common.hpp>

#include <cstring>
#include <iostream>

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
      0x333333, 0xFFFFFF, 0x000000,
      /* last two are system foreground and system background */
  };
  return color_index;
}

std::optional<HorizontalAlign>
read_horizontal(const pugi::xml_attribute attribute) {
  if (const std::string value = attribute.value(); value == "center") {
    return HorizontalAlign::center;
  }
  return {};
}

std::optional<VerticalAlign>
read_vertical(const pugi::xml_attribute attribute) {
  if (const std::string value = attribute.value(); value == "center") {
    return VerticalAlign::middle;
  }
  return {};
}

std::optional<Color> read_color(const pugi::xml_node node) {
  if (const pugi::xml_attribute indexed = node.attribute("indexed")) {
    return color_index().at(indexed.as_uint());
  }
  if (const pugi::xml_attribute rgb = node.attribute("rgb")) {
    const char *value = rgb.value();
    if (std::strlen(value) == 8) {
      const std::uint32_t color = std::strtoull(value, nullptr, 16);
      return Color(color, false);
    }
    if (std::strlen(value) == 6) {
      const std::uint32_t color = std::strtoull(value, nullptr, 16);
      return Color(color);
    }
  }
  return {};
}

std::optional<std::string> read_border(const pugi::xml_node node) {
  if (!node) {
    return {};
  }
  std::string result;
  if (!node.attribute("style")) {
    return {};
  }
  // TODO: thin only
  result.append("0.75pt solid ");
  if (const std::optional<Color> color = read_color(node.child("color"))) {
    result.append(html::color(*color));
  }
  return result;
}

} // namespace

StyleRegistry::StyleRegistry() = default;

StyleRegistry::StyleRegistry(const pugi::xml_node styles_root) {
  generate_indices_(styles_root);
}

ResolvedStyle StyleRegistry::cell_style(const std::uint32_t i) const {
  ResolvedStyle result;

  const pugi::xml_node cell_format = m_cell_formats_index.at(i);

  if (const pugi::xml_attribute font_id = cell_format.attribute("fontId");
      cell_format.attribute("applyFont").as_bool() && font_id) {
    resolve_font_(font_id.as_uint(), result);
  }

  if (const pugi::xml_attribute fill_id = cell_format.attribute("fillId")) {
    resolve_fill_(fill_id.as_uint(), result);
  }

  if (const pugi::xml_attribute border_id = cell_format.attribute("borderId");
      cell_format.attribute("applyBorder").as_bool() && border_id) {
    resolve_border_(border_id.as_uint(), result);
  }

  if (const pugi::xml_node alignment = cell_format.child("alignment");
      cell_format.attribute("applyAlignment").as_bool() && alignment) {
    result.table_cell_style.horizontal_align =
        read_horizontal(alignment.attribute("horizontal"));
    result.table_cell_style.vertical_align =
        read_vertical(alignment.attribute("vertical"));
    if (auto text_rotation = alignment.attribute("textRotation").as_float();
        text_rotation != 0) {
      result.table_cell_style.text_rotation = text_rotation;
    }
  }

  if (const pugi::xml_node protection = cell_format.child("protection");
      cell_format.attribute("applyProtection").as_bool() && protection) {
    // TODO
  }

  return result;
}

void StyleRegistry::resolve_font_(const std::uint32_t i,
                                  ResolvedStyle &result) const {
  const pugi::xml_node font = m_fonts_index.at(i);

  if (const pugi::xml_attribute size = font.child("sz").attribute("val")) {
    result.text_style.font_size = Measure(size.as_float(), DynamicUnit("pt"));
  }
  if (const pugi::xml_attribute font_name =
          font.child("name").attribute("val")) {
    result.text_style.font_name = font_name.value();
  }
  if (font.child("b")) {
    result.text_style.font_weight = FontWeight::bold;
  }
  if (const pugi::xml_node color = font.child("color")) {
    result.text_style.font_color = read_color(color);
  }
}

void StyleRegistry::resolve_fill_(const std::uint32_t i,
                                  ResolvedStyle &result) const {
  const pugi::xml_node fill = m_fills_index.at(i);

  if (const pugi::xml_node pattern = fill.child("patternFill")) {
    result.table_cell_style.background_color =
        read_color(pattern.child("bgColor"));
  }
}

void StyleRegistry::resolve_border_(const std::uint32_t i,
                                    ResolvedStyle &result) const {
  const pugi::xml_node border = m_borders_index.at(i);

  result.table_cell_style.border.right = read_border(border.child("right"));
  result.table_cell_style.border.top = read_border(border.child("top"));
  result.table_cell_style.border.left = read_border(border.child("left"));
  result.table_cell_style.border.bottom = read_border(border.child("bottom"));
}

void StyleRegistry::generate_indices_(const pugi::xml_node styles_root) {
  for (const pugi::xml_node font : styles_root.child("fonts")) {
    m_fonts_index.push_back(font);
  }

  for (const pugi::xml_node fill : styles_root.child("fills")) {
    m_fills_index.push_back(fill);
  }

  for (const pugi::xml_node border : styles_root.child("borders")) {
    m_borders_index.push_back(border);
  }

  for (const pugi::xml_node cell_master : styles_root.child("cellStyleXfs")) {
    m_cell_masters_index.push_back(cell_master);
  }

  for (const pugi::xml_node cell_format : styles_root.child("cellXfs")) {
    m_cell_formats_index.push_back(cell_format);
  }
}

} // namespace odr::internal::ooxml::spreadsheet
