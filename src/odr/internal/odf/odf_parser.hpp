#pragma once

#include <odr/document_element_identifier.hpp>

namespace pugi {
class xml_node;
} // namespace pugi

namespace odr::internal::odf {
class ElementRegistry;

ExtendedElementIdentifier parse_tree(ElementRegistry &registry,
                                     pugi::xml_node node);

} // namespace odr::internal::odf
