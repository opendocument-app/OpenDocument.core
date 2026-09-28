#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_style.hpp>

#include <odr/internal/html/common.hpp>
#include <odr/internal/ooxml/ooxml_util.hpp>
#include <odr/internal/xml/xml_util.hpp>

#include <array>
#include <cstdlib>
#include <span>
#include <sstream>
#include <string>
#include <string_view>

#include <fmt/format.h>

namespace odr::internal::ooxml::spreadsheet {

namespace {

const std::vector<Color> &color_index() {
  using namespace odr;

  static const std::vector color_index{
      0x000000_rgb, 0xFFFFFF_rgb, 0xFF0000_rgb, 0x00FF00_rgb, 0x0000FF_rgb,
      0xFFFF00_rgb, 0xFF00FF_rgb, 0x00FFFF_rgb, 0x000000_rgb, 0xFFFFFF_rgb,
      0xFF0000_rgb, 0x00FF00_rgb, 0x0000FF_rgb, 0xFFFF00_rgb, 0xFF00FF_rgb,
      0x00FFFF_rgb, 0x800000_rgb, 0x008000_rgb, 0x000080_rgb, 0x808000_rgb,
      0x800080_rgb, 0x008080_rgb, 0xC0C0C0_rgb, 0x808080_rgb, 0x9999FF_rgb,
      0x993366_rgb, 0xFFFFCC_rgb, 0xCCFFFF_rgb, 0x660066_rgb, 0xFF8080_rgb,
      0x0066CC_rgb, 0xCCCCFF_rgb, 0x000080_rgb, 0xFF00FF_rgb, 0xFFFF00_rgb,
      0x00FFFF_rgb, 0x800080_rgb, 0x800000_rgb, 0x008080_rgb, 0x0000FF_rgb,
      0x00CCFF_rgb, 0xCCFFFF_rgb, 0xCCFFCC_rgb, 0xFFFF99_rgb, 0x99CCFF_rgb,
      0xFF99CC_rgb, 0xCC99FF_rgb, 0xFFCC99_rgb, 0x3366FF_rgb, 0x33CCCC_rgb,
      0x99CC00_rgb, 0xFFCC00_rgb, 0xFF9900_rgb, 0xFF6600_rgb, 0x666699_rgb,
      0x969696_rgb, 0x003366_rgb, 0x339966_rgb, 0x003300_rgb, 0x333300_rgb,
      0x993300_rgb, 0x993366_rgb, 0x333399_rgb, 0x333333_rgb, 0xFFFFFF_rgb,
      0x000000_rgb,
      /* last two are system foreground and system background */
  };
  return color_index;
}

std::optional<HorizontalAlign>
read_horizontal(const pugi::xml_attribute attribute) {
  const std::string_view value = attribute.value();
  if (value == "left") {
    return HorizontalAlign::left;
  }
  if (value == "center") {
    return HorizontalAlign::center;
  }
  if (value == "right") {
    return HorizontalAlign::right;
  }
  return {};
}

std::optional<VerticalAlign>
read_vertical(const pugi::xml_attribute attribute) {
  const std::string_view value = attribute.value();
  if (value == "top") {
    return VerticalAlign::top;
  }
  if (value == "center") {
    return VerticalAlign::middle;
  }
  if (value == "bottom") {
    return VerticalAlign::bottom;
  }
  return {};
}

/// A `CT_BooleanProperty`: present is on, unless `val` says otherwise.
bool read_toggle(const pugi::xml_node node) {
  return node && node.attribute("val").as_bool(true);
}

/// The child @p name of @p parent, made at its place in @p order.
pugi::xml_node ordered_child(pugi::xml_node parent, const char *name,
                             const std::span<const std::string_view> order) {
  if (const pugi::xml_node existing = parent.child(name)) {
    return existing;
  }
  return xml::insert_in_sequence(parent, name, order);
}

/// The @p name child of `styleSheet`, made in the order [ECMA-376] 18.8.39
/// gives them where it is missing.
pugi::xml_node collection_of(pugi::xml_node root, const char *name) {
  static constexpr std::array<std::string_view, 11> order{
      "numFmts",      "fonts",   "fills",      "borders",
      "cellStyleXfs", "cellXfs", "cellStyles", "dxfs",
      "tableStyles",  "colors",  "extLst"};
  return ordered_child(root, name, order);
}

std::string argb_of(const Color &color) { return "FF" + hex_color(color); }

/// [ECMA-376] 18.8.22 `CT_Font` is a sequence.
constexpr std::array<std::string_view, 15> font_order{
    "b",         "i",  "strike", "condense", "extend", "outline", "shadow", "u",
    "vertAlign", "sz", "color",  "name",     "family", "charset", "scheme"};

/// `val` is dropped for on, as Excel writes it, and is `0` for off.
void set_toggle(pugi::xml_node font, const char *name, const bool on) {
  pugi::xml_node node = ordered_child(font, name, font_order);
  node.remove_attribute("val");
  if (!on) {
    node.append_attribute("val").set_value("0");
  }
}

std::string serialized(const pugi::xml_node node) {
  std::ostringstream out;
  node.print(out, "", pugi::format_raw);
  return out.str();
}

/// The index of the child of @p collection equal to @p node, which is
/// appended where none is, with the `count` kept.
std::uint32_t intern(pugi::xml_node collection, const pugi::xml_node node) {
  const std::string wanted = serialized(node);
  std::uint32_t index = 0;
  for (const pugi::xml_node child : collection.children()) {
    if (serialized(child) == wanted) {
      return index;
    }
    ++index;
  }
  collection.append_copy(node);
  xml::set_attribute(collection, "count", std::to_string(index + 1).c_str());
  return index;
}

/// What an empty `styleSheet` lacks and every `xf` needs a first entry of.
void state_defaults(pugi::xml_node root) {
  if (pugi::xml_node fonts = collection_of(root, "fonts");
      !fonts.first_child()) {
    pugi::xml_node font = fonts.append_child("font");
    font.append_child("sz").append_attribute("val").set_value("11");
    font.append_child("name").append_attribute("val").set_value("Calibri");
    xml::set_attribute(fonts, "count", "1");
  }
  if (pugi::xml_node fills = collection_of(root, "fills");
      !fills.first_child()) {
    // 18.8.21: the first two are reserved
    fills.append_child("fill")
        .append_child("patternFill")
        .append_attribute("patternType")
        .set_value("none");
    fills.append_child("fill")
        .append_child("patternFill")
        .append_attribute("patternType")
        .set_value("gray125");
    xml::set_attribute(fills, "count", "2");
  }
  if (pugi::xml_node borders = collection_of(root, "borders");
      !borders.first_child()) {
    borders.append_child("border");
    xml::set_attribute(borders, "count", "1");
  }
  const auto default_xf = [](pugi::xml_node xf) {
    xf.append_attribute("numFmtId").set_value("0");
    xf.append_attribute("fontId").set_value("0");
    xf.append_attribute("fillId").set_value("0");
    xf.append_attribute("borderId").set_value("0");
    return xf;
  };
  if (pugi::xml_node masters = collection_of(root, "cellStyleXfs");
      !masters.first_child()) {
    default_xf(masters.append_child("xf"));
    xml::set_attribute(masters, "count", "1");
  }
  if (pugi::xml_node formats = collection_of(root, "cellXfs");
      !formats.first_child()) {
    default_xf(formats.append_child("xf"))
        .append_attribute("xfId")
        .set_value("0");
    xml::set_attribute(formats, "count", "1");
  }
}

} // namespace

StyleRegistry::StyleRegistry() = default;

StyleRegistry::StyleRegistry(const pugi::xml_node styles_root,
                             const pugi::xml_node theme_root)
    : m_styles_root{styles_root} {
  const pugi::xml_node scheme =
      theme_root.child("a:themeElements").child("a:clrScheme");
  for (const char *name : {"a:lt1", "a:dk1", "a:lt2", "a:dk2", "a:accent1",
                           "a:accent2", "a:accent3", "a:accent4", "a:accent5",
                           "a:accent6", "a:hlink", "a:folHlink"}) {
    m_theme_colors.push_back(read_drawing_rgb_color(scheme.child(name)));
  }

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
    result.table_cell_style.wrap_text =
        alignment.attribute("wrapText").as_bool();
    if (const float text_rotation =
            alignment.attribute("textRotation").as_float();
        text_rotation != 0) {
      result.table_cell_style.text_rotation = text_rotation;
    }
  }

  // TODO `protection` (locked/hidden) is read out and dropped; nothing in
  // `TableCellStyle` carries it, and the render has no lock to show.
  if (const pugi::xml_node protection = cell_format.child("protection");
      cell_format.attribute("applyProtection").as_bool() && protection) {
  }

  return result;
}

/// [ECMA-376] 18.8.3 `CT_Color`: `rgb`, a legacy `indexed` slot or a `theme`
/// slot, any of them moved by `tint`.
std::optional<Color>
StyleRegistry::read_color_(const pugi::xml_node node) const {
  std::optional<Color> result;
  if (const pugi::xml_attribute theme = node.attribute("theme")) {
    if (theme.as_uint() < m_theme_colors.size()) {
      result = m_theme_colors[theme.as_uint()];
    }
  } else if (const pugi::xml_attribute indexed = node.attribute("indexed")) {
    if (indexed.as_uint() < color_index().size()) {
      result = color_index()[indexed.as_uint()];
    }
  } else if (const pugi::xml_attribute rgb = node.attribute("rgb")) {
    const std::string_view value = rgb.value();
    // the alpha byte is not one: excel ignores it and producers routinely
    // write `00`, which would paint nothing at all
    if (value.size() == 8 || value.size() == 6) {
      result = Color::from_rgb(static_cast<std::uint32_t>(
          std::strtoull(rgb.value(), nullptr, 16) & 0xffffff));
    }
  }
  if (const pugi::xml_attribute tint = node.attribute("tint"); result && tint) {
    result = apply_tint(*result, tint.as_double());
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
  if (const pugi::xml_node bold = font.child("b")) {
    result.text_style.font_weight =
        read_toggle(bold) ? FontWeight::bold : FontWeight::normal;
  }
  if (const pugi::xml_node italic = font.child("i")) {
    result.text_style.font_style =
        read_toggle(italic) ? FontStyle::italic : FontStyle::normal;
  }
  if (const pugi::xml_node underline = font.child("u")) {
    result.text_style.font_underline =
        std::string_view(underline.attribute("val").value()) != "none";
  }
  if (const pugi::xml_node strike = font.child("strike")) {
    result.text_style.font_line_through = read_toggle(strike);
  }
  if (const pugi::xml_node color = font.child("color")) {
    result.text_style.font_color = read_color_(color);
  }
}

/// [ECMA-376] 18.8.32: a pattern paints `fgColor`, so a solid fill is that
/// colour; `none`, the default, paints nothing.
void StyleRegistry::resolve_fill_(const std::uint32_t i,
                                  ResolvedStyle &result) const {
  const pugi::xml_node pattern = m_fills_index.at(i).child("patternFill");
  const std::string_view type = pattern.attribute("patternType").value();
  if (!pattern || type.empty() || type == "none") {
    return;
  }
  result.table_cell_style.background_color =
      read_color_(pattern.child("fgColor"));
}

void StyleRegistry::resolve_border_(const std::uint32_t i,
                                    ResolvedStyle &result) const {
  const pugi::xml_node border = m_borders_index.at(i);

  const auto side =
      [&](const pugi::xml_node node) -> std::optional<std::string> {
    if (!node.attribute("style")) {
      return {};
    }
    // TODO: thin only
    std::string declaration = "0.75pt solid ";
    if (const std::optional<Color> color = read_color_(node.child("color"))) {
      declaration.append(html::color(*color));
    }
    return declaration;
  };
  result.table_cell_style.border.right = side(border.child("right"));
  result.table_cell_style.border.top = side(border.child("top"));
  result.table_cell_style.border.left = side(border.child("left"));
  result.table_cell_style.border.bottom = side(border.child("bottom"));
}

std::uint32_t
StyleRegistry::create_cell_format(const std::uint32_t base,
                                  const TableCellStyle &cell_style,
                                  const TextStyle &text_style) {
  state_defaults(m_styles_root);
  generate_indices_(m_styles_root);

  pugi::xml_document scratch;
  pugi::xml_node xf = scratch.append_copy(
      m_cell_formats_index.at(base < m_cell_formats_index.size() ? base : 0));

  if (text_style.font_weight || text_style.font_style ||
      text_style.font_underline || text_style.font_line_through ||
      text_style.font_color || text_style.font_size) {
    const std::uint32_t font_id = xf.attribute("fontId").as_uint();
    pugi::xml_node font = scratch.append_copy(
        m_fonts_index.at(font_id < m_fonts_index.size() ? font_id : 0));
    if (text_style.font_weight) {
      set_toggle(font, "b", *text_style.font_weight == FontWeight::bold);
    }
    if (text_style.font_style) {
      set_toggle(font, "i", *text_style.font_style == FontStyle::italic);
    }
    if (text_style.font_line_through) {
      set_toggle(font, "strike", *text_style.font_line_through);
    }
    if (text_style.font_underline) {
      pugi::xml_node underline = ordered_child(font, "u", font_order);
      underline.remove_attribute("val");
      if (!*text_style.font_underline) {
        underline.append_attribute("val").set_value("none");
      }
    }
    if (text_style.font_size) {
      xml::set_attribute(
          ordered_child(font, "sz", font_order), "val",
          fmt::format("{:g}", points(*text_style.font_size)).c_str());
    }
    if (text_style.font_color) {
      pugi::xml_node color = ordered_child(font, "color", font_order);
      color.remove_attributes();
      color.append_attribute("rgb").set_value(
          argb_of(*text_style.font_color).c_str());
    }
    xml::set_attribute(
        xf, "fontId",
        std::to_string(intern(m_styles_root.child("fonts"), font)).c_str());
    xml::set_attribute(xf, "applyFont", "1");
  }

  if (cell_style.background_color) {
    pugi::xml_node fill = scratch.append_child("fill");
    pugi::xml_node pattern = fill.append_child("patternFill");
    if (cell_style.background_color->alpha == 0) {
      pattern.append_attribute("patternType").set_value("none");
    } else {
      pattern.append_attribute("patternType").set_value("solid");
      pattern.append_child("fgColor").append_attribute("rgb").set_value(
          argb_of(*cell_style.background_color).c_str());
      pattern.append_child("bgColor").append_attribute("indexed").set_value(
          "64");
    }
    xml::set_attribute(
        xf, "fillId",
        std::to_string(intern(m_styles_root.child("fills"), fill)).c_str());
    xml::set_attribute(xf, "applyFill", "1");
  }

  if (cell_style.horizontal_align) {
    pugi::xml_node alignment = xf.child("alignment");
    if (!alignment) {
      alignment = xf.prepend_child("alignment");
    }
    const char *horizontal = "left";
    if (*cell_style.horizontal_align == HorizontalAlign::center) {
      horizontal = "center";
    } else if (*cell_style.horizontal_align == HorizontalAlign::right) {
      horizontal = "right";
    }
    xml::set_attribute(alignment, "horizontal", horizontal);
    xml::set_attribute(xf, "applyAlignment", "1");
  }

  const std::uint32_t result = intern(m_styles_root.child("cellXfs"), xf);
  generate_indices_(m_styles_root);
  return result;
}

void StyleRegistry::generate_indices_(const pugi::xml_node styles_root) {
  m_fonts_index.clear();
  m_fills_index.clear();
  m_borders_index.clear();
  m_cell_masters_index.clear();
  m_cell_formats_index.clear();

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
