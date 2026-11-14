#include <odr/internal/ooxml/presentation/ooxml_presentation_style.hpp>

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/ooxml/ooxml_util.hpp>
#include <odr/internal/xml/xml_util.hpp>

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace odr::internal::ooxml::presentation {

namespace {

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
    if (const std::optional<Color> color = read_drawing_rgb_color(slot);
        color.has_value()) {
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
  if (const std::optional<Color> color = read_drawing_rgb_color(parent);
      color.has_value()) {
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
    std::unordered_map<std::string, Theme> &themes,
    std::unordered_map<std::string, pugi::xml_document> &parts) {
  LayoutStyle result;

  // `unordered_map` keeps its elements put, so the nodes and the theme
  // pointer outlive the inserts that follow
  const auto resident = [&](const AbsPath &path) {
    const auto [it, inserted] = parts.try_emplace(path.string());
    if (inserted && !parse_optional_part(files, path, it->second)) {
      it->second.reset();
    }
    return it->second.document_element();
  };

  result.layout = resident(layout_path);
  if (!result.layout) {
    return result;
  }

  const std::optional<AbsPath> master_path =
      parse_relationship_target(files, layout_path, "slideMaster");
  if (master_path.has_value()) {
    result.master = resident(*master_path);
  }
  if (master_path.has_value() && result.master) {
    const auto [theme_it, inserted] = themes.try_emplace(master_path->string());
    if (inserted) {
      pugi::xml_document theme;
      if (const std::optional<AbsPath> theme_path =
              parse_relationship_target(files, *master_path, "theme");
          theme_path.has_value() &&
          parse_optional_part(files, *theme_path, theme)) {
        const pugi::xml_node elements =
            theme.document_element().child("a:themeElements");
        theme_it->second.color_scheme = ColorScheme(
            elements.child("a:clrScheme"), result.master.child("p:clrMap"));
        const pugi::xml_node fonts = elements.child("a:fontScheme");
        theme_it->second.major_font = fonts.child("a:majorFont")
                                          .child("a:latin")
                                          .attribute("typeface")
                                          .value();
        theme_it->second.minor_font = fonts.child("a:minorFont")
                                          .child("a:latin")
                                          .attribute("typeface")
                                          .value();
      }
    }
    result.theme = &theme_it->second;
    result.background =
        read_background_color(result.master, &result.theme->color_scheme)
            .value_or(std::optional<Color>());
  }

  if (const std::optional<std::optional<Color>> stated = read_background_color(
          result.layout,
          result.theme == nullptr ? nullptr : &result.theme->color_scheme);
      stated.has_value()) {
    result.background = *stated;
  }
  return result;
}

presentation::PlaceholderSources
presentation::find_placeholder_sources(const pugi::xml_node shape,
                                       const pugi::xml_node layout,
                                       const pugi::xml_node master) {
  PlaceholderSources result;
  const pugi::xml_node placeholder =
      shape.child("p:nvSpPr").child("p:nvPr").child("p:ph");
  if (!placeholder) {
    return result;
  }

  // [ECMA-376] 19.7.10 `ST_PlaceholderType`: absent is `obj`
  const std::string_view type = placeholder.attribute("type")
                                    ? placeholder.attribute("type").value()
                                    : "obj";
  const bool is_title = type == "title" || type == "ctrTitle";
  const pugi::xml_attribute index = placeholder.attribute("idx");

  const auto placeholder_of = [](const pugi::xml_node candidate) {
    return candidate.child("p:nvSpPr").child("p:nvPr").child("p:ph");
  };
  const auto type_of = [](const pugi::xml_node ph) -> std::string_view {
    return ph.attribute("type") ? ph.attribute("type").value() : "obj";
  };
  const auto find = [&](const pugi::xml_node part, const auto &matches) {
    for (const pugi::xml_node candidate :
         part.child("p:cSld").child("p:spTree").children("p:sp")) {
      if (const pugi::xml_node ph = placeholder_of(candidate);
          ph && matches(ph)) {
        return candidate;
      }
    }
    return pugi::xml_node();
  };

  if (!is_title && index) {
    result.layout_shape = find(layout, [&](const pugi::xml_node ph) {
      return std::string_view(ph.attribute("idx").value()) == index.value();
    });
  }
  if (!result.layout_shape) {
    result.layout_shape = find(
        layout, [&](const pugi::xml_node ph) { return type_of(ph) == type; });
  }
  if (!result.layout_shape && is_title) {
    result.layout_shape = find(layout, [&](const pugi::xml_node ph) {
      return type_of(ph) == "title" || type_of(ph) == "ctrTitle";
    });
  }

  // a master holds one placeholder per kind: the title, the body that every
  // other text placeholder follows, and the date, footer and number
  const std::string_view master_type =
      is_title                                            ? "title"
      : type == "dt" || type == "ftr" || type == "sldNum" ? type
                                                          : "body";
  result.master_shape = find(master, [&](const pugi::xml_node ph) {
    return type_of(ph) == master_type;
  });
  result.master_list_style =
      master.child("p:txStyles")
          .child(is_title ? "p:titleStyle" : "p:bodyStyle");
  return result;
}

void presentation::resolve_text_properties(const pugi::xml_node run_properties,
                                           const Theme *theme,
                                           TextStyle &result) {
  const ColorScheme *color_scheme =
      theme == nullptr ? nullptr : &theme->color_scheme;

  if (const pugi::xml_attribute font_name =
          run_properties.child("a:latin").attribute("typeface")) {
    // [ECMA-376] 20.1.4.1.24: `+mj-lt` and `+mn-lt` name the theme's fonts
    const std::string_view name = font_name.value();
    if (!name.starts_with('+')) {
      result.font_name = std::string(name);
    } else if (theme != nullptr && name == "+mj-lt" &&
               !theme->major_font.empty()) {
      result.font_name = theme->major_font;
    } else if (theme != nullptr && name == "+mn-lt" &&
               !theme->minor_font.empty()) {
      result.font_name = theme->minor_font;
    }
  }
  if (const std::optional<Measure> font_size =
          read_hundredth_point_attribute(run_properties.attribute("sz"));
      font_size.has_value()) {
    result.font_size = font_size;
  }
  if (const std::optional<FontWeight> font_weight =
          read_font_weight_attribute(run_properties.attribute("b"));
      font_weight.has_value()) {
    result.font_weight = font_weight;
  }
  if (const std::optional<FontStyle> font_style =
          read_font_style_attribute(run_properties.attribute("i"));
      font_style.has_value()) {
    result.font_style = font_style;
  }
  if (const std::optional<bool> font_underline =
          read_line_attribute(run_properties.attribute("u"));
      font_underline.has_value()) {
    result.font_underline = font_underline;
  }
  if (const std::optional<bool> font_line_through =
          read_line_attribute(run_properties.attribute("strike"));
      font_line_through.has_value()) {
    result.font_line_through = font_line_through;
  }
  if (const std::optional<std::string> font_shadow =
          read_shadow_attribute(run_properties.attribute("shadow"));
      font_shadow.has_value()) {
    result.font_shadow = font_shadow;
  }
  if (const std::optional<Color> font_color =
          read_drawing_color(run_properties.child("a:solidFill"), color_scheme);
      font_color.has_value()) {
    result.font_color = font_color;
  }
  if (const std::optional<Color> background_color =
          read_drawing_color(run_properties.child("a:highlight"), color_scheme);
      background_color.has_value()) {
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

void presentation::resolve_paragraph_properties(
    const pugi::xml_node paragraph_properties, ParagraphStyle &result) {
  if (const std::optional<TextAlign> text_align =
          read_drawing_text_align_attribute(
              paragraph_properties.attribute("algn"));
      text_align.has_value()) {
    result.text_align = text_align;
  }
  if (const std::optional<TextDirection> direction =
          read_drawing_text_direction_attribute(
              paragraph_properties.attribute("rtl"));
      direction.has_value()) {
    result.direction = direction;
  }
  if (const std::optional<Measure> margin_left =
          read_emus_attribute(paragraph_properties.attribute("marL"));
      margin_left.has_value()) {
    result.margin.left = margin_left;
  }
  if (const std::optional<Measure> margin_right =
          read_emus_attribute(paragraph_properties.attribute("marR"));
      margin_right.has_value()) {
    result.margin.right = margin_right;
  }
  if (const std::optional<Measure> line_height =
          read_line_spacing(paragraph_properties.child("a:lnSpc"));
      line_height.has_value()) {
    result.line_height = line_height;
  }
  // absolute only: a percent here is of the text size, which css would
  // resolve against the width instead
  if (const std::optional<Measure> margin_top =
          read_hundredth_point_attribute(paragraph_properties.child("a:spcBef")
                                             .child("a:spcPts")
                                             .attribute("val"));
      margin_top.has_value()) {
    result.margin.top = margin_top;
  }
  if (const std::optional<Measure> margin_bottom =
          read_hundredth_point_attribute(paragraph_properties.child("a:spcAft")
                                             .child("a:spcPts")
                                             .attribute("val"));
      margin_bottom.has_value()) {
    result.margin.bottom = margin_bottom;
  }
}

} // namespace odr::internal::ooxml
