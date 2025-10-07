#pragma once

namespace pugi {
class xml_node;
} // namespace pugi

namespace odr {
class ExtendedElementIdentifier;
}

namespace odr::internal::odf {
class ElementRegistry;

ExtendedElementIdentifier parse_tree(ElementRegistry &registry,
                                     pugi::xml_node node);

} // namespace odr::internal::odf
