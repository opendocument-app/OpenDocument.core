#ifndef ODR_INTERNAL_OOXML_UTIL_H
#define ODR_INTERNAL_OOXML_UTIL_H

#include <any>
#include <debug/unordered_map>
#include <functional>
#include <odr/style.h>
#include <optional>
#include <pugixml.hpp>
#include <string>
#include <unordered_map>

namespace pugi {
class xml_attribute;
class xml_document;
class xml_node;
} // namespace pugi

namespace odr::internal::abstract {
class ReadableFilesystem;
} // namespace odr::internal::abstract

namespace odr::internal::common {
class Path;
} // namespace odr::internal::common

namespace odr::internal::ooxml {

std::optional<std::string> read_string_attribute(pugi::xml_attribute attribute);
std::optional<Color> read_color_attribute(pugi::xml_attribute attribute);
std::optional<Measure> read_half_point_attribute(pugi::xml_attribute attribute);
std::optional<Measure> read_emus_attribute(pugi::xml_attribute attribute);
std::optional<Measure> read_twips_attribute(pugi::xml_attribute attribute);
std::optional<Measure> read_width_attribute(pugi::xml_node attribute);
bool read_line_attribute(pugi::xml_node attribute);
std::optional<std::string> read_shadow_attribute(pugi::xml_node node);
std::optional<FontWeight> read_font_weight_attribute(pugi::xml_node node);
std::optional<FontStyle> read_font_style_attribute(pugi::xml_node node);
std::optional<TextAlign> read_text_align_attribute(pugi::xml_node node);
std::optional<VerticalAlign>
read_vertical_align_attribute(pugi::xml_attribute attribute);
std::optional<std::string> read_border_attribute(pugi::xml_node node);

std::string read_text_property(pugi::xml_node node);

std::unordered_map<std::string, std::string>
parse_relationships(const pugi::xml_document &relations);
std::unordered_map<std::string, std::string>
parse_relationships(const abstract::ReadableFilesystem &filesystem,
                    const common::Path &path);

} // namespace odr::internal::ooxml

#endif // ODR_INTERNAL_OOXML_UTIL_H
