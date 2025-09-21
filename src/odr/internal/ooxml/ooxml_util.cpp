#include <odr/internal/ooxml/ooxml_util.hpp>

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/html/common.hpp>
#include <odr/internal/util/string_util.hpp>
#include <odr/internal/util/xml_util.hpp>

#include <cstring>

namespace odr::internal {

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
    return Color(color);
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

  // handle percentage which is not always a percentage for tables
  // http://officeopenxml.com/WPtableWidth.php
  // potentially this should be moved to a table parser

  std::string val = attribute.value();
  util::string::trim(val);

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

bool ooxml::read_line_attribute(const pugi::xml_node node) {
  if (!node) {
    return false;
  }
  if (const char *val = node.attribute("w:val").value();
      std::strcmp("none", val) == 0 || std::strcmp("false", val) == 0 ||
      std::strcmp("noStrike", val) == 0) {
    return false;
  }
  return true;
}

bool ooxml::read_line_attribute(const pugi::xml_attribute attribute) {
  if (!attribute) {
    return false;
  }
  if (const char *val = attribute.value(); std::strcmp("none", val) == 0 ||
                                           std::strcmp("false", val) == 0 ||
                                           std::strcmp("noStrike", val) == 0) {
    return false;
  }
  return true;
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
  if (const char *val = node.attribute("w:val").value();
      std::strcmp("false", val) == 0 || std::strcmp("0", val) == 0) {
    return FontWeight::normal;
  }
  return FontWeight::bold;
}

std::optional<FontWeight>
ooxml::read_font_weight_attribute(const pugi::xml_attribute attribute) {
  if (!attribute) {
    return {};
  }
  if (const char *val = attribute.value();
      std::strcmp("false", val) == 0 || std::strcmp("0", val) == 0) {
    return FontWeight::normal;
  }
  return FontWeight::bold;
}

std::optional<FontStyle>
ooxml::read_font_style_attribute(const pugi::xml_node node) {
  if (!node) {
    return {};
  }
  if (const char *val = node.attribute("w:val").value();
      std::strcmp("false", val) == 0) {
    return {};
  }
  return FontStyle::italic;
}

std::optional<FontStyle>
ooxml::read_font_style_attribute(const pugi::xml_attribute attribute) {
  if (!attribute) {
    return {};
  }
  if (const char *val = attribute.value(); std::strcmp("false", val) == 0) {
    return {};
  }
  return FontStyle::italic;
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

std::optional<std::string> ooxml::read_border_node(const pugi::xml_node node) {
  if (!node) {
    return {};
  }
  const char *val = node.attribute("w:val").value();
  if (std::strcmp("nil", val) == 0) {
    return {};
  }
  std::string result;
  result
      .append(
          read_half_point_attribute(node.attribute("w:sz")).value().to_string())
      .append(" ");
  if (std::strcmp("none", val) == 0) {
    result.append(node.attribute("w:val").value()).append(" ");
  } else {
    result.append("solid ");
  }
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

std::unordered_map<std::string, std::string>
ooxml::parse_relationships(const abstract::ReadableFilesystem &filesystem,
                           const AbsPath &path) {
  const AbsPath rel_path = path.parent()
                               .join(RelPath("_rels"))
                               .join(RelPath(path.basename() + ".rels"));
  if (!filesystem.is_file(rel_path)) {
    return {};
  }

  const pugi::xml_document relationships =
      util::xml::parse(filesystem, rel_path);
  return parse_relationships(relationships);
}

} // namespace odr::internal
