#pragma once

#include <odr/definitions.hpp>

namespace pugi {
class xml_node;
} // namespace pugi

namespace odr::internal::odf {
class ElementRegistry;

ElementIdentifier parse_tree(ElementRegistry &registry, pugi::xml_node node);

} // namespace odr::internal::odf
