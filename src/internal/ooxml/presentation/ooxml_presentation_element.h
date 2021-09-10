#ifndef ODR_INTERNAL_OOXML_PRESENTATION_ELEMENT_H
#define ODR_INTERNAL_OOXML_PRESENTATION_ELEMENT_H

#include <internal/abstract/document.h>
#include <internal/common/style.h>
#include <pugixml.hpp>

namespace odr::internal::ooxml::presentation {
class Document;

class Element : public virtual abstract::Element {
public:
  Element(const Document *document, pugi::xml_node node);

  [[nodiscard]] bool equals(const abstract::Document *document,
                            const abstract::DocumentCursor *cursor,
                            const abstract::Element &rhs) const override;

  abstract::Element *parent(const abstract::Document *document,
                            const abstract::DocumentCursor *cursor,
                            const abstract::Allocator *allocator) override;
  abstract::Element *first_child(const abstract::Document *document,
                                 const abstract::DocumentCursor *cursor,
                                 const abstract::Allocator *allocator) override;
  abstract::Element *
  previous_sibling(const abstract::Document *document,
                   const abstract::DocumentCursor *cursor,
                   const abstract::Allocator *allocator) override;
  abstract::Element *
  next_sibling(const abstract::Document *document,
               const abstract::DocumentCursor *cursor,
               const abstract::Allocator *allocator) override;

  common::ResolvedStyle partial_style(const abstract::Document *document) const;
  common::ResolvedStyle
  intermediate_style(const abstract::Document *document,
                     const abstract::DocumentCursor *cursor) const;

protected:
  pugi::xml_node m_node;

  static const Document *document_(const abstract::Document *document);
  static pugi::xml_node root_(const abstract::Document *document);
  static pugi::xml_node slide_(const abstract::Document *document,
                               const std::string &id);
};

abstract::Element *
construct_default_element(const Document *document, pugi::xml_node node,
                          const abstract::Allocator *allocator);
abstract::Element *
construct_default_parent_element(const Document *document, pugi::xml_node node,
                                 const abstract::Allocator *allocator);
abstract::Element *
construct_default_first_child_element(const Document *document,
                                      pugi::xml_node node,
                                      const abstract::Allocator *allocator);
abstract::Element *construct_default_previous_sibling_element(
    const Document *document, pugi::xml_node node,
    const abstract::Allocator *allocator);
abstract::Element *
construct_default_next_sibling_element(const Document *document,
                                       pugi::xml_node node,
                                       const abstract::Allocator *allocator);

} // namespace odr::internal::ooxml::presentation

#endif // ODR_INTERNAL_OOXML_PRESENTATION_ELEMENT_H
