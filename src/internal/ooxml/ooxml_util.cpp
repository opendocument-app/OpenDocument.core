#include <cstdint>
#include <cstring>
#include <internal/abstract/filesystem.h>
#include <internal/common/html.h>
#include <internal/common/path.h>
#include <internal/ooxml/ooxml_util.h>
#include <internal/util/xml_util.h>
#include <iterator>
#include <odr/quantity.h>
#include <odr/style.h>
#include <pugixml.hpp>
#include <stdlib.h>
#include <utility>

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
  static const std::unordered_map<std::string, Color> color_map{
      {"red", {255, 0, 0}},
      {"green", {0, 255, 0}},
      {"blue", {0, 0, 255}},
  };

  if (!attribute) {
    return {};
  }
  auto value = attribute.value();
  if (std::strcmp("auto", attribute.value()) == 0) {
    return {};
  }
  if (auto color_map_it = color_map.find(value);
      color_map_it != std::end(color_map)) {
    return color_map_it->second;
  }
  if (std::strlen(value) == 6) {
    std::uint32_t color = std::strtoull(value, nullptr, 16);
    return Color((std::uint8_t)(color >> 16), (std::uint8_t)(color >> 8),
                 (std::uint8_t)(color >> 0));
  }
  return {};
}

std::optional<Measure>
ooxml::read_half_point_attribute(const pugi::xml_attribute attribute) {
  if (!attribute) {
    return {};
  }
  return Measure(attribute.as_float() * 0.5f, DynamicUnit("pt"));
}

std::optional<Measure>
ooxml::read_emus_attribute(const pugi::xml_attribute attribute) {
  if (!attribute) {
    return {};
  }
  return Measure(attribute.as_float() / 914400.0f, DynamicUnit("in"));
}

std::optional<Measure>
ooxml::read_twips_attribute(const pugi::xml_attribute attribute) {
  if (!attribute) {
    return {};
  }
  return Measure(attribute.as_float() / 1440.0f, DynamicUnit("in"));
}

std::optional<Measure> ooxml::read_width_attribute(const pugi::xml_node node) {
  if (!node) {
    return {};
  }
  auto type = node.attribute("w:type").value();
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
    return Measure(node.attribute("w:w").as_float(), DynamicUnit("%"));
  }
  return {};
}

bool ooxml::read_line_attribute(const pugi::xml_node node) {
  if (!node) {
    return false;
  }
  auto val = node.attribute("w:val").value();
  if (std::strcmp("none", val) == 0) {
    return false;
  }
  if (std::strcmp("false", val) == 0) {
    return {};
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

std::optional<FontWeight>
ooxml::read_font_weight_attribute(const pugi::xml_node node) {
  if (!node) {
    return {};
  }
  auto val = node.attribute("w:val").value();
  if (std::strcmp("false", val) == 0) {
    return {};
  }
  return FontWeight::bold;
}

std::optional<FontStyle>
ooxml::read_font_style_attribute(const pugi::xml_node node) {
  if (!node) {
    return {};
  }
  auto val = node.attribute("w:val").value();
  if (std::strcmp("false", val) == 0) {
    return {};
  }
  return FontStyle::italic;
}

std::optional<TextAlign>
ooxml::read_text_align_attribute(const pugi::xml_node node) {
  auto val = node.attribute("w:val").value();
  if ((std::strcmp("left", val) == 0) || (std::strcmp("start", val) == 0)) {
    return TextAlign::left;
  }
  if ((std::strcmp("right", val) == 0) || (std::strcmp("end", val) == 0)) {
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
  auto val = attribute.value();
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

std::optional<std::string>
ooxml::read_border_attribute(const pugi::xml_node node) {
  if (!node) {
    return {};
  }
  auto val = node.attribute("w:val").value();
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
  if (auto color = read_color_attribute(node.attribute("w:color"))) {
    result.append(common::html::color(*color));
  }
  return result;
}

std::unordered_map<std::string, std::string>
ooxml::parse_relationships(const pugi::xml_document &rels) {
  std::unordered_map<std::string, std::string> result;
  for (auto &&e : rels.select_nodes("//Relationship")) {
    const std::string r_id = e.node().attribute("Id").as_string();
    const std::string p = e.node().attribute("Target").as_string();
    result.insert({r_id, p});
  }
  return result;
}

std::unordered_map<std::string, std::string>
ooxml::parse_relationships(const abstract::ReadableFilesystem &filesystem,
                           const common::Path &path) {
  const auto rel_path =
      path.parent().join("_rels").join(path.basename() + ".rels");
  if (!filesystem.is_file(rel_path)) {
    return {};
  }

  const auto relationships = util::xml::parse(filesystem, rel_path);
  return parse_relationships(relationships);
}

} // namespace odr::internal
