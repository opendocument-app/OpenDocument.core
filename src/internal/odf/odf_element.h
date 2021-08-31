#ifndef ODR_INTERNAL_ODF_ELEMENT_H
#define ODR_INTERNAL_ODF_ELEMENT_H

#include <internal/abstract/document.h>
#include <pugixml.hpp>

namespace odr::internal::odf {
class Document;
class Style;

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

std::unordered_map<ElementProperty, std::any> fetch_properties(
    const std::unordered_map<std::string, ElementProperty> &property_table,
    pugi::xml_node node, const Style *style, ElementType element_type,
    const char *default_style_name = nullptr);

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_ELEMENT_H
