#ifndef ODR_INTERNAL_OOXML_UTIL_H
#define ODR_INTERNAL_OOXML_UTIL_H

#include <any>
#include <string>
#include <unordered_map>

namespace pugi {
class xml_document;
class xml_node;
class xml_attribute;
} // namespace pugi

namespace odr::internal::abstract {
class ReadableFilesystem;
} // namespace odr::internal::abstract

namespace odr::internal::common {
class Path;
} // namespace odr::internal::common

namespace odr::internal::ooxml {

std::any read_color_attribute(pugi::xml_attribute attribute);
std::any read_half_point_attribute(pugi::xml_attribute attribute);
std::any read_emus_attribute(pugi::xml_attribute attribute);
std::any read_line_attribute(pugi::xml_attribute attribute);
std::any read_shadow_attribute(pugi::xml_node node);

std::unordered_map<std::string, std::string>
parse_relationships(const pugi::xml_document &relations);
std::unordered_map<std::string, std::string>
parse_relationships(const abstract::ReadableFilesystem &filesystem,
                    const common::Path &path);

} // namespace odr::internal::ooxml

#endif // ODR_INTERNAL_OOXML_UTIL_H
