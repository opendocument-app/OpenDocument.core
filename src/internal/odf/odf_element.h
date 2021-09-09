#ifndef ODR_INTERNAL_ODF_ELEMENT_H
#define ODR_INTERNAL_ODF_ELEMENT_H

#include <internal/abstract/document.h>
#include <pugixml.hpp>

namespace odr::internal::odf {
class Document;

class Element : public virtual abstract::Element {
public:
  Element(const Document *, pugi::xml_node node);

  [[nodiscard]] bool equals(const abstract::Document *,
                            const abstract::DocumentCursor *,
                            const abstract::Element &rhs) const override;

  abstract::Element *parent(const abstract::Document *document,
                            const abstract::DocumentCursor *,
                            const abstract::Allocator *allocator) override;
  abstract::Element *first_child(const abstract::Document *document,
                                 const abstract::DocumentCursor *,
                                 const abstract::Allocator *allocator) override;
  abstract::Element *
  previous_sibling(const abstract::Document *document,
                   const abstract::DocumentCursor *,
                   const abstract::Allocator *allocator) override;
  abstract::Element *
  next_sibling(const abstract::Document *document,
               const abstract::DocumentCursor *,
               const abstract::Allocator *allocator) override;

  ResolvedStyle element_style(const abstract::Document *document) const;

protected:
  pugi::xml_node m_node;

  static const Document *document_(const abstract::Document *document);
  static const StyleRegistry *style_(const abstract::Document *document);
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

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_ELEMENT_H
