#include <internal/abstract/filesystem.h>
#include <internal/common/path.h>
#include <internal/ooxml/ooxml_util.h>
#include <internal/util/xml_util.h>
#include <pugixml.hpp>

namespace odr::internal {

std::any ooxml::read_color_attribute(const pugi::xml_attribute attribute) {
  std::string value = attribute.value();
  if (value == "auto") {
    return {};
  }
  if (value.length() == 6) {
    return "#" + value;
  }
  return value;
}

std::any ooxml::read_half_point_attribute(const pugi::xml_attribute attribute) {
  return std::to_string(attribute.as_float() * 0.5f) + "pt";
}

std::any ooxml::read_emus_attribute(const pugi::xml_attribute attribute) {
  return std::to_string(attribute.as_float() / 914400.0f) + "in";
}

std::any ooxml::read_line_attribute(const pugi::xml_attribute attribute) {
  const std::string value = attribute.value();
  if (value == "none") {
    return {};
  }
  // TODO
  return std::any("solid");
}

std::any ooxml::read_shadow_attribute(const pugi::xml_node) {
  // TODO
  return "1pt 1pt";
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
