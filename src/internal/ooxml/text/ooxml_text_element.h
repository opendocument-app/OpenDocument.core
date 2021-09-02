#ifndef ODR_INTERNAL_OOXML_TEXT_ELEMENT_H
#define ODR_INTERNAL_OOXML_TEXT_ELEMENT_H

#include <internal/abstract/document.h>
#include <pugixml.hpp>

namespace odr::internal::ooxml::text {
class Document;

class Element;

using Allocator = abstract::Element::Allocator;

abstract::Element *construct_default_element(const Document *document,
                                             pugi::xml_node node,
                                             const Allocator &allocator);

} // namespace odr::internal::ooxml::text

#endif // ODR_INTERNAL_OOXML_TEXT_ELEMENT_H
