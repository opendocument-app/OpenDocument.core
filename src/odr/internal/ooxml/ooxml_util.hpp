#ifndef ODR_INTERNAL_OOXML_UTIL_HPP
#define ODR_INTERNAL_OOXML_UTIL_HPP

#include <odr/style.hpp>

#include <any>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

#include <pugixml.hpp>

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

std::optional<std::string> read_string_attribute(pugi::xml_attribute);
std::optional<Color> read_color_attribute(pugi::xml_attribute);
std::optional<Measure> read_half_point_attribute(pugi::xml_attribute);
std::optional<Measure> read_hundredth_point_attribute(pugi::xml_attribute);
std::optional<Measure> read_emus_attribute(pugi::xml_attribute);
std::optional<Measure> read_twips_attribute(pugi::xml_attribute);
std::optional<Measure> read_width_attribute(pugi::xml_node);
bool read_line_attribute(pugi::xml_attribute);
bool read_line_attribute(pugi::xml_node);
std::optional<std::string> read_shadow_attribute(pugi::xml_attribute);
std::optional<std::string> read_shadow_attribute(pugi::xml_node);
std::optional<FontWeight> read_font_weight_attribute(pugi::xml_attribute);
std::optional<FontWeight> read_font_weight_attribute(pugi::xml_node);
std::optional<FontStyle> read_font_style_attribute(pugi::xml_attribute);
std::optional<FontStyle> read_font_style_attribute(pugi::xml_node);
std::optional<TextAlign> read_text_align_attribute(pugi::xml_attribute);
std::optional<VerticalAlign> read_vertical_align_attribute(pugi::xml_attribute);
std::optional<std::string> read_border_node(pugi::xml_node);

std::string read_text_property(pugi::xml_node);

using Relations = std::unordered_map<std::string, std::string>;

std::unordered_map<std::string, std::string>
parse_relationships(const pugi::xml_document &relations);
std::unordered_map<std::string, std::string>
parse_relationships(const abstract::ReadableFilesystem &filesystem,
                    const common::Path &path);

} // namespace odr::internal::ooxml

#endif // ODR_INTERNAL_OOXML_UTIL_HPP
