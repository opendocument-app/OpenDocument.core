#include <odr/internal/ooxml/ooxml_util.hpp>

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/html/common.hpp>
#include <odr/internal/util/string_util.hpp>
#include <odr/internal/util/xml_util.hpp>

#include <cstring>
#include <stdexcept>

namespace odr::internal {

namespace {

// value readers shared by the `w:val`-node and the bare-attribute overloads

bool line_from_value(const char *value) {
  return std::strcmp("none", value) != 0 && std::strcmp("false", value) != 0 &&
         std::strcmp("noStrike", value) != 0;
}

std::optional<FontWeight> font_weight_from_value(const char *value) {
  if (std::strcmp("false", value) == 0 || std::strcmp("0", value) == 0) {
    return FontWeight::normal;
  }
  return FontWeight::bold;
}

std::optional<FontStyle> font_style_from_value(const char *value) {
  if (std::strcmp("false", value) == 0) {
    return {};
  }
  return FontStyle::italic;
}

} // namespace

std::optional<std::string>
ooxml::read_string_attribute(const pugi::xml_attribute attribute) {
  if (!attribute) {
    return {};
  }
  return attribute.value();
}

std::optional<Color>
ooxml::read_color_attribute(const pugi::xml_attribute attribute) {
  // color codes from http://officeopenxml.com/WPtextShading.php
  // rgb values suggested by chatgpt
  static const std::unordered_map<std::string, Color> color_map{
      {"black", {0, 0, 0}},       {"blue", {0, 112, 192}},
      {"cyan", {0, 176, 240}},    {"darkBlue", {0, 32, 96}},
      {"darkCyan", {0, 97, 133}}, {"darkGray", {64, 64, 64}},
      {"darkGreen", {0, 128, 0}}, {"darkMagenta", {112, 48, 160}},
      {"darkRed", {192, 0, 0}},   {"darkYellow", {128, 96, 0}},
      {"green", {0, 176, 80}},    {"lightGray", {191, 191, 191}},
      {"magenta", {255, 0, 255}}, {"red", {255, 0, 0}},
      {"white", {255, 255, 255}}, {"yellow", {255, 255, 0}},
  };

  if (!attribute) {
    return {};
  }
  const char *value = attribute.value();
  if (std::strcmp("auto", value) == 0 || std::strcmp("none", value) == 0) {
    return {};
  }
  if (const auto color_map_it = color_map.find(value);
      color_map_it != std::end(color_map)) {
    return color_map_it->second;
  }
  if (std::strlen(value) == 6) {
    const std::uint32_t color = std::strtoull(value, nullptr, 16);
    return Color::from_rgb(color);
  }
  return {};
}

std::optional<Measure>
ooxml::read_half_point_attribute(const pugi::xml_attribute attribute) {
  if (!attribute) {
    return {};
  }
  return Measure(attribute.as_double() * 0.5, DynamicUnit("pt"));
}

std::optional<Measure>
ooxml::read_hundredth_point_attribute(const pugi::xml_attribute attribute) {
  if (!attribute) {
    return {};
  }
  return Measure(attribute.as_double() * 0.01, DynamicUnit("pt"));
}

std::optional<Measure>
ooxml::read_emus_attribute(const pugi::xml_attribute attribute) {
  if (!attribute) {
    return {};
  }
  return Measure(attribute.as_double() / 914400.0, DynamicUnit("in"));
}

std::optional<Measure>
ooxml::read_twips_attribute(const pugi::xml_attribute attribute) {
  if (!attribute) {
    return {};
  }
  return Measure(attribute.as_double() / 1440.0, DynamicUnit("in"));
}

std::optional<Measure>
ooxml::read_pct_attribute(const pugi::xml_attribute attribute) {
  if (!attribute) {
    return {};
  }

  // a table width in "pct" is in fiftieths of a percent unless it carries a
  // literal `%` — http://officeopenxml.com/WPtableWidth.php
  std::string val = attribute.value();
  util::string::trim_inplace(val);

  if (val.find('%') != std::string::npos) {
    util::string::replace_all(val, "%", "");
    return Measure(std::stod(val), DynamicUnit("%"));
  }

  return Measure(attribute.as_double() / 50.0, DynamicUnit("%"));
}

std::optional<Measure> ooxml::read_width_attribute(const pugi::xml_node node) {
  if (!node) {
    return {};
  }
  const char *type = node.attribute("w:type").value();
  if (std::strcmp("auto", type) == 0) {
    return {};
  }
  if (std::strcmp("dxa", type) == 0) {
    return read_twips_attribute(node.attribute("w:w"));
  }
  if (std::strcmp("nil", type) == 0) {
    return Measure(0, DynamicUnit(""));
  }
  if (std::strcmp("pct", type) == 0) {
    return read_pct_attribute(node.attribute("w:w"));
  }
  return {};
}

/// [ECMA-376] 17.17.4 ST_OnOff, as an attribute that is off when absent.
bool ooxml::read_on_off_attribute(const pugi::xml_attribute attribute) {
  if (!attribute) {
    return false;
  }
  const char *value = attribute.value();
  return std::strcmp("0", value) != 0 && std::strcmp("false", value) != 0 &&
         std::strcmp("off", value) != 0;
}

/// [ECMA-376] 17.17.4 ST_OnOff, as an element that is on unless `w:val` says
/// otherwise.
bool ooxml::read_on_off_attribute(const pugi::xml_node node) {
  if (!node) {
    return false;
  }
  const pugi::xml_attribute value = node.attribute("w:val");
  if (!value) {
    return true;
  }
  return read_on_off_attribute(value);
}

bool ooxml::read_line_attribute(const pugi::xml_node node) {
  if (!node) {
    return false;
  }
  return line_from_value(node.attribute("w:val").value());
}

bool ooxml::read_line_attribute(const pugi::xml_attribute attribute) {
  if (!attribute) {
    return false;
  }
  return line_from_value(attribute.value());
}

std::optional<std::string>
ooxml::read_shadow_attribute(const pugi::xml_node node) {
  if (!node) {
    return {};
  }
  return "1pt 1pt";
}

std::optional<std::string>
ooxml::read_shadow_attribute(const pugi::xml_attribute attribute) {
  if (!attribute) {
    return {};
  }
  return "1pt 1pt";
}

std::optional<FontWeight>
ooxml::read_font_weight_attribute(const pugi::xml_node node) {
  if (!node) {
    return {};
  }
  return font_weight_from_value(node.attribute("w:val").value());
}

std::optional<FontWeight>
ooxml::read_font_weight_attribute(const pugi::xml_attribute attribute) {
  if (!attribute) {
    return {};
  }
  return font_weight_from_value(attribute.value());
}

std::optional<FontStyle>
ooxml::read_font_style_attribute(const pugi::xml_node node) {
  if (!node) {
    return {};
  }
  return font_style_from_value(node.attribute("w:val").value());
}

std::optional<FontStyle>
ooxml::read_font_style_attribute(const pugi::xml_attribute attribute) {
  if (!attribute) {
    return {};
  }
  return font_style_from_value(attribute.value());
}

std::optional<TextAlign>
ooxml::read_text_align_attribute(const pugi::xml_attribute attribute) {
  const char *val = attribute.value();
  if (std::strcmp("left", val) == 0 || std::strcmp("start", val) == 0) {
    return TextAlign::left;
  }
  if (std::strcmp("right", val) == 0 || std::strcmp("end", val) == 0) {
    return TextAlign::right;
  }
  if (std::strcmp("center", val) == 0) {
    return TextAlign::center;
  }
  if (std::strcmp("justify", val) == 0) {
    return TextAlign::justify;
  }
  return {};
}

/// [ECMA-376] 20.1.10.59 ST_TextAlignType — drawingml spells the same values
/// differently than wordprocessingml does.
std::optional<TextAlign>
ooxml::read_drawing_text_align_attribute(const pugi::xml_attribute attribute) {
  const char *val = attribute.value();
  if (std::strcmp("l", val) == 0) {
    return TextAlign::left;
  }
  if (std::strcmp("r", val) == 0) {
    return TextAlign::right;
  }
  if (std::strcmp("ctr", val) == 0) {
    return TextAlign::center;
  }
  if (std::strcmp("just", val) == 0) {
    return TextAlign::justify;
  }
  return {};
}

std::optional<VerticalAlign>
ooxml::read_vertical_align_attribute(const pugi::xml_attribute attribute) {
  const char *val = attribute.value();
  if (std::strcmp("top", val) == 0) {
    return VerticalAlign::top;
  }
  if (std::strcmp("center", val) == 0) {
    return VerticalAlign::middle;
  }
  if (std::strcmp("bottom", val) == 0) {
    return VerticalAlign::bottom;
  }
  return {};
}

/// [ECMA-376] 20.1.10.60 ST_TextAnchoringType — drawingml spells the same
/// values differently than wordprocessingml does.
std::optional<VerticalAlign> ooxml::read_drawing_vertical_align_attribute(
    const pugi::xml_attribute attribute) {
  const char *val = attribute.value();
  if (std::strcmp("t", val) == 0) {
    return VerticalAlign::top;
  }
  if (std::strcmp("ctr", val) == 0) {
    return VerticalAlign::middle;
  }
  if (std::strcmp("b", val) == 0) {
    return VerticalAlign::bottom;
  }
  return {};
}

std::optional<std::string> ooxml::read_border_node(const pugi::xml_node node) {
  if (!node) {
    return {};
  }
  const char *val = node.attribute("w:val").value();
  if (std::strcmp("nil", val) == 0) {
    return {};
  }
  const std::optional<Measure> size =
      read_half_point_attribute(node.attribute("w:sz"));
  if (!size.has_value()) {
    return {};
  }
  std::string result;
  result.append(size->to_string()).append(" ");
  result.append(std::strcmp("none", val) == 0 ? "none " : "solid ");
  if (const std::optional<Color> color =
          read_color_attribute(node.attribute("w:color"))) {
    result.append(html::color(*color));
  }
  return result;
}

std::unordered_map<std::string, std::string>
ooxml::parse_relationships(const pugi::xml_document &relations) {
  std::unordered_map<std::string, std::string> result;
  for (const pugi::xpath_node e : relations.select_nodes("//Relationship")) {
    const std::string r_id = e.node().attribute("Id").as_string();
    const std::string p = e.node().attribute("Target").as_string();
    result.insert({r_id, p});
  }
  return result;
}

namespace {

AbsPath relationships_path(const AbsPath &path) {
  return path.parent()
      .join(RelPath("_rels"))
      .join(RelPath(path.basename() + ".rels"));
}

/// [ECMA-376] 15.2.4: a target is relative to the part that states it, unless
/// it names a part from the package root. One that is empty, or that climbs out
/// of the package, names nothing.
std::optional<AbsPath> resolve_relationship_target(const AbsPath &path,
                                                   const char *target) {
  if (target == nullptr || *target == '\0') {
    return {};
  }
  if (*target == '/') {
    return AbsPath(target);
  }
  try {
    return path.parent().join(RelPath(target));
  } catch (const std::invalid_argument &) {
    return {};
  }
}

} // namespace

std::unordered_map<std::string, std::string>
ooxml::parse_relationships(const abstract::ReadableFilesystem &filesystem,
                           const AbsPath &path) {
  const AbsPath rel_path = relationships_path(path);
  if (!filesystem.is_file(rel_path)) {
    return {};
  }

  const pugi::xml_document relationships =
      util::xml::parse(filesystem, rel_path);
  return parse_relationships(relationships);
}

/// The target of the first relationship whose type ends in @p type
/// (`slideLayout`, `slideMaster`, `theme`, …), resolved against the part.
std::optional<AbsPath>
ooxml::parse_relationship_target(const abstract::ReadableFilesystem &filesystem,
                                 const AbsPath &path,
                                 const std::string_view type) {
  const AbsPath rel_path = relationships_path(path);
  if (!filesystem.is_file(rel_path)) {
    return {};
  }

  const pugi::xml_document relationships =
      util::xml::parse(filesystem, rel_path);
  for (const pugi::xpath_node e :
       relationships.select_nodes("//Relationship")) {
    // the type is a uri, so `type` has to match a whole trailing segment
    const std::string_view relation_type =
        e.node().attribute("Type").as_string();
    if (!relation_type.ends_with(type) || relation_type.size() == type.size() ||
        relation_type[relation_type.size() - type.size() - 1] != '/') {
      continue;
    }
    return resolve_relationship_target(
        path, e.node().attribute("Target").as_string());
  }
  return {};
}

} // namespace odr::internal
