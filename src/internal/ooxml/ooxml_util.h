#ifndef ODR_INTERNAL_OOXML_UTIL_H
#define ODR_INTERNAL_OOXML_UTIL_H

#include <any>
#include <functional>
#include <pugixml.hpp>
#include <string>
#include <unordered_map>

namespace odr::internal::abstract {
class ReadableFilesystem;
} // namespace odr::internal::abstract

namespace odr::internal::common {
class Path;
} // namespace odr::internal::common

namespace odr::internal::ooxml {

std::any read_string_attribute(pugi::xml_attribute attribute);
std::any read_color_attribute(pugi::xml_attribute attribute);
std::any read_half_point_attribute(pugi::xml_attribute attribute);
std::any read_emus_attribute(pugi::xml_attribute attribute);
std::any read_line_attribute(pugi::xml_attribute attribute);
std::any read_shadow_attribute(pugi::xml_node node);

std::any read_text_property(pugi::xml_node node);

using NodeTransformation =
    std::function<std::any(const pugi::xml_node attribute)>;
using AttributeTransformation =
    std::function<std::any(const pugi::xml_attribute attribute)>;

std::any read_optional_node(pugi::xml_node node,
                            const NodeTransformation &node_transformation);
std::any read_optional_attribute(
    pugi::xml_attribute attribute,
    const AttributeTransformation &attribute_transformation =
        read_string_attribute);

std::unordered_map<std::string, std::string>
parse_relationships(const pugi::xml_document &relations);
std::unordered_map<std::string, std::string>
parse_relationships(const abstract::ReadableFilesystem &filesystem,
                    const common::Path &path);

} // namespace odr::internal::ooxml

#endif // ODR_INTERNAL_OOXML_UTIL_H
