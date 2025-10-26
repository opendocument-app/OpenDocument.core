#pragma once

namespace pugi {
class xml_node;
}

namespace odr {
class ExtendedElementIdentifier;
}

namespace odr::internal::ooxml::presentation {
class ElementRegistry;

ExtendedElementIdentifier parse_tree(ElementRegistry &registry,
                                     pugi::xml_node node);

} // namespace odr::internal::ooxml::presentation
