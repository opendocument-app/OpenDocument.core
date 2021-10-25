#include <internal/ooxml/spreadsheet/ooxml_spreadsheet_style.h>

namespace odr::internal::ooxml::spreadsheet {

namespace {

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
    if (!result.table_cell_style) {
      result.table_cell_style = TableCellStyle();
    }

    read_vertical(alignment.attribute("horizontal")); // TODO
    result.table_cell_style->vertical_align =
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

  if (!result.text_style) {
    result.text_style = TextStyle();
  }

  if (auto size = font.child("sz").attribute("val")) {
    result.text_style->font_size = Measure(size.as_float(), DynamicUnit("pt"));
  }
  if (auto font_name = font.child("name").attribute("val")) {
    result.text_style->font_name = font_name.value();
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
