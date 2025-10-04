#pragma once

#include <odr/document_element_identifier.hpp>

#include <tuple>

namespace pugi {
class xml_node;
} // namespace pugi

namespace odr::internal::odf {
class ElementRegistry;

ElementIdentifier parse_tree(ElementRegistry &entry_registry,
                             pugi::xml_node node);

std::tuple<ElementIdentifier, pugi::xml_node>
parse_any_element_tree(ElementRegistry &entry_registry, pugi::xml_node node);

void parse_element_children(ElementRegistry &entry_registry,
                            ElementIdentifier parent_id, pugi::xml_node node);

} // namespace odr::internal::odf
