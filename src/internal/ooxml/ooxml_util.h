#ifndef ODR_INTERNAL_OOXML_UTIL_H
#define ODR_INTERNAL_OOXML_UTIL_H

#include <any>

namespace pugi {
class xml_node;
class xml_attribute;
} // namespace pugi

namespace odr::internal::ooxml {

std::any read_color_attribute(pugi::xml_attribute attribute);
std::any read_half_point_attribute(pugi::xml_attribute attribute);
std::any read_line_attribute(pugi::xml_attribute attribute);
std::any read_shadow_attribute(pugi::xml_node node);

} // namespace odr::internal::ooxml

#endif // ODR_INTERNAL_OOXML_UTIL_H
