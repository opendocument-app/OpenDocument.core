#ifndef ODR_INTERNAL_OOXML_TEXT_ELEMENT_H
#define ODR_INTERNAL_OOXML_TEXT_ELEMENT_H

#include <internal/abstract/document.h>
#include <internal/common/style.h>
#include <pugixml.hpp>
#include <string>

namespace odr::internal::ooxml::text {
class Document;
class StyleRegistry;
class Style;

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

  virtual common::ResolvedStyle
  partial_style(const abstract::Document *document) const;
  virtual common::ResolvedStyle
  intermediate_style(const abstract::Document *document,
                     const abstract::DocumentCursor *cursor) const;

protected:
  pugi::xml_node m_node;

  static const Document *document_(const abstract::Document *document);
  static const StyleRegistry *style_(const abstract::Document *document);
  static const std::unordered_map<std::string, std::string> &
  document_relations_(const abstract::Document *document);

  friend class Style;
};

abstract::Element *
construct_default_element(pugi::xml_node node, const Document *document,
                          const abstract::Allocator *allocator);

} // namespace odr::internal::ooxml::text

#endif // ODR_INTERNAL_OOXML_TEXT_ELEMENT_H
