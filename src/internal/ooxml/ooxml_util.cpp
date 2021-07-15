#include <internal/ooxml/ooxml_util.h>
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
  return std::to_string(attribute.as_int() * 0.5f) + "pt";
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

} // namespace odr::internal
