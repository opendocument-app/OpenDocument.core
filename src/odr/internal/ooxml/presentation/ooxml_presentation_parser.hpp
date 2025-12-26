#pragma once

#include <odr/definitions.hpp>

#include <string>
#include <unordered_map>

#include <pugixml.hpp>

namespace odr::internal::ooxml::presentation {
class ElementRegistry;

ElementIdentifier parse_tree(
    ElementRegistry &registry, pugi::xml_node node,
    const std::unordered_map<std::string, pugi::xml_document> &slides_xml);

} // namespace odr::internal::ooxml::presentation
