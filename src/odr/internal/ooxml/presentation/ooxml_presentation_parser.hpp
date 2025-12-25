#pragma once

#include <odr/definitions.hpp>

namespace pugi {
class xml_node;
}

namespace odr::internal::ooxml::presentation {
class ElementRegistry;

ElementIdentifier parse_tree(ElementRegistry &registry, pugi::xml_node node);

} // namespace odr::internal::ooxml::presentation
