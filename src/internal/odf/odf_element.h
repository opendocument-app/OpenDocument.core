#ifndef ODR_INTERNAL_ODF_ELEMENT_H
#define ODR_INTERNAL_ODF_ELEMENT_H

#include <internal/abstract/document.h>
#include <pugixml.hpp>

namespace odr::internal::odf {
class Document;

class Element;

using Allocator = abstract::Element::Allocator;

abstract::Element *construct_default_element(const Document *document,
                                             pugi::xml_node node,
                                             const Allocator &allocator);
abstract::Element *construct_default_parent_element(const Document *document,
                                                    pugi::xml_node node,
                                                    const Allocator &allocator);
abstract::Element *construct_default_first_child_element(
    const Document *document, pugi::xml_node node, const Allocator &allocator);
abstract::Element *construct_default_previous_sibling_element(
    const Document *document, pugi::xml_node node, const Allocator &allocator);
abstract::Element *construct_default_next_sibling_element(
    const Document *document, pugi::xml_node node, const Allocator &allocator);

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_ELEMENT_H
