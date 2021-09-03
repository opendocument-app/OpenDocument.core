#ifndef ODR_INTERNAL_OOXML_TEXT_ELEMENT_H
#define ODR_INTERNAL_OOXML_TEXT_ELEMENT_H

#include <internal/abstract/document.h>
#include <pugixml.hpp>

namespace odr::internal::ooxml::text {
class Document;
class StyleRegistry;
class Style;

class Element : public abstract::Element {
public:
  explicit Element(pugi::xml_node node);

protected:
  pugi::xml_node m_node;

  static const Document *document_(const abstract::Document *document);
  static const StyleRegistry *style_(const abstract::Document *document);

  friend class Style;
};

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

} // namespace odr::internal::ooxml::text

#endif // ODR_INTERNAL_OOXML_TEXT_ELEMENT_H
