#include <odr/internal/ooxml/presentation/ooxml_presentation_style.hpp>

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/ooxml/ooxml_util.hpp>
#include <odr/internal/xml/xml_util.hpp>

#include <stdexcept>
#include <string_view>
#include <utility>

namespace odr::internal::ooxml::presentation {

namespace {

/// A literal colour, or a system colour that names the value it last resolved
/// to. [ECMA-376] 20.1.2.3.32, 20.1.2.3.33
std::optional<Color> read_theme_color_(const pugi::xml_node slot) {
  if (const std::optional<Color> color =
          read_color_attribute(slot.child("a:srgbClr").attribute("val"))) {
    return color;
  }
  return read_color_attribute(slot.child("a:sysClr").attribute("lastClr"));
}

/// `a:spcPct` in thousandths of a percent, or `a:spcPts` in hundredths of a
/// point. [ECMA-376] 21.1.2.2.12
std::optional<Measure> read_line_spacing(const pugi::xml_node node) {
  if (const pugi::xml_attribute percent =
          node.child("a:spcPct").attribute("val")) {
    return Measure(percent.as_double() * 1e-3, DynamicUnit("%"));
  }
  return read_hundredth_point_attribute(
      node.child("a:spcPts").attribute("val"));
}

/// Styling is optional, so a part that is missing or is not xml leaves a slide
/// without it rather than failing the open.
bool parse_optional_part(const abstract::ReadableFilesystem &files,
                         const AbsPath &path, pugi::xml_document &result) {
  if (!files.is_file(path)) {
    return false;
  }
  try {
    result = xml::parse(files, path);
  } catch (const std::exception &) {
    return false;
  }
  return true;
}

} // namespace

} // namespace odr::internal::ooxml::presentation

namespace odr::internal::ooxml {

presentation::ColorScheme::ColorScheme(const pugi::xml_node color_scheme,
                                       const pugi::xml_node color_map) {
  for (const pugi::xml_node slot : color_scheme.children()) {
    if (const std::optional<Color> color = read_theme_color_(slot)) {
      // the slot names are `a:dk1`, `a:lt1`, `a:accent1`, …
      const std::string_view name = slot.name();
      const std::size_t colon = name.find(':');
      m_colors[std::string(
          colon == std::string_view::npos ? name : name.substr(colon + 1))] =
          *color;
    }
  }
  // a mapping's source may be another's target, so every one of them resolves
  // against the theme rather than against a map it is being written to
  std::unordered_map<std::string, Color> mapped = m_colors;
  for (const pugi::xml_attribute mapping : color_map.attributes()) {
    if (const auto it = m_colors.find(mapping.value());
        it != std::end(m_colors)) {
      mapped[mapping.name()] = it->second;
    }
  }
  m_colors = std::move(mapped);
}

std::optional<Color>
presentation::ColorScheme::resolve(const char *name) const {
  const auto it = m_colors.find(name);
  return it == std::end(m_colors) ? std::optional<Color>() : it->second;
}

std::optional<Color>
presentation::read_drawing_color(const pugi::xml_node parent,
                                 const ColorScheme *color_scheme) {
  if (const std::optional<Color> color = read_theme_color_(parent)) {
    return color;
  }
  if (const pugi::xml_attribute scheme_color =
          parent.child("a:schemeClr").attribute("val");
      scheme_color && color_scheme != nullptr) {
    return color_scheme->resolve(scheme_color.value());
  }
  return {};
}

std::optional<std::optional<Color>>
presentation::read_background_color(const pugi::xml_node slide_like,
                                    const ColorScheme *color_scheme) {
  const pugi::xml_node background = slide_like.child("p:cSld").child("p:bg");
  if (!background) {
    return {};
  }
  return read_drawing_color(background.child("p:bgPr").child("a:solidFill"),
                            color_scheme);
}

presentation::LayoutStyle presentation::load_layout_style(
    const abstract::ReadableFilesystem &files, const AbsPath &layout_path,
    std::unordered_map<std::string, ColorScheme> &color_schemes) {
  LayoutStyle result;

  pugi::xml_document layout;
  if (!parse_optional_part(files, layout_path, layout)) {
    return result;
  }

  pugi::xml_document master;
  if (const std::optional<AbsPath> master_path =
          parse_relationship_target(files, layout_path, "slideMaster");
      master_path.has_value() &&
      parse_optional_part(files, *master_path, master)) {
    // `unordered_map` keeps its elements put, so the pointer outlives the
    // inserts that follow
    const auto [master_it, inserted] =
        color_schemes.try_emplace(master_path->string());
    if (inserted) {
      pugi::xml_document theme;
      if (const std::optional<AbsPath> theme_path =
              parse_relationship_target(files, *master_path, "theme");
          theme_path.has_value() &&
          parse_optional_part(files, *theme_path, theme)) {
        master_it->second =
            ColorScheme(theme.document_element()
                            .child("a:themeElements")
                            .child("a:clrScheme"),
                        master.document_element().child("p:clrMap"));
      }
    }
    result.color_scheme = &master_it->second;
    result.background =
        read_background_color(master.document_element(), result.color_scheme)
            .value_or(std::optional<Color>());
  }

  if (const std::optional<std::optional<Color>> stated = read_background_color(
          layout.document_element(), result.color_scheme)) {
    result.background = *stated;
  }
  return result;
}

void presentation::resolve_text_style(const pugi::xml_node node,
                                      const ColorScheme *color_scheme,
                                      TextStyle &result) {
  const pugi::xml_node run_properties = node.child("a:rPr");

  if (const pugi::xml_attribute font_name =
          run_properties.child("a:latin").attribute("typeface")) {
    result.font_name = font_name.value();
  }
  if (const std::optional<Measure> font_size =
          read_hundredth_point_attribute(run_properties.attribute("sz"))) {
    result.font_size = font_size;
  }
  if (const std::optional<FontWeight> font_weight =
          read_font_weight_attribute(run_properties.attribute("b"))) {
    result.font_weight = font_weight;
  }
  if (const std::optional<FontStyle> font_style =
          read_font_style_attribute(run_properties.attribute("i"))) {
    result.font_style = font_style;
  }
  if (const bool font_underline =
          read_line_attribute(run_properties.attribute("u"))) {
    result.font_underline = font_underline;
  }
  if (const bool font_line_through =
          read_line_attribute(run_properties.attribute("strike"))) {
    result.font_line_through = font_line_through;
  }
  if (const std::optional<std::string> font_shadow =
          read_shadow_attribute(run_properties.attribute("shadow"))) {
    result.font_shadow = font_shadow;
  }
  if (const std::optional<Color> font_color = read_drawing_color(
          run_properties.child("a:solidFill"), color_scheme)) {
    result.font_color = font_color;
  }
  if (const std::optional<Color> background_color = read_drawing_color(
          run_properties.child("a:highlight"), color_scheme)) {
    result.background_color = background_color;
  }
  // the sign carries the direction
  if (const pugi::xml_attribute baseline =
          run_properties.attribute("baseline")) {
    result.font_position = baseline.as_int() > 0   ? FontPosition::super
                           : baseline.as_int() < 0 ? FontPosition::sub
                                                   : FontPosition::normal;
  }
}

void presentation::resolve_paragraph_style(const pugi::xml_node node,
                                           ParagraphStyle &result) {
  const pugi::xml_node paragraph_properties = node.child("a:pPr");

  if (const std::optional<TextAlign> text_align =
          read_drawing_text_align_attribute(
              paragraph_properties.attribute("algn"))) {
    result.text_align = text_align;
  }
  if (const std::optional<Measure> margin_left =
          read_emus_attribute(paragraph_properties.attribute("marL"))) {
    result.margin.left = margin_left;
  }
  if (const std::optional<Measure> margin_right =
          read_emus_attribute(paragraph_properties.attribute("marR"))) {
    result.margin.right = margin_right;
  }
  if (const std::optional<Measure> line_height =
          read_line_spacing(paragraph_properties.child("a:lnSpc"))) {
    result.line_height = line_height;
  }
  // absolute only: a percent here is of the text size, which css would
  // resolve against the width instead
  if (const std::optional<Measure> margin_top =
          read_hundredth_point_attribute(paragraph_properties.child("a:spcBef")
                                             .child("a:spcPts")
                                             .attribute("val"))) {
    result.margin.top = margin_top;
  }
  if (const std::optional<Measure> margin_bottom =
          read_hundredth_point_attribute(paragraph_properties.child("a:spcAft")
                                             .child("a:spcPts")
                                             .attribute("val"))) {
    result.margin.bottom = margin_bottom;
  }
}

} // namespace odr::internal::ooxml
