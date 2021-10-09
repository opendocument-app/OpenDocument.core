#ifndef ODR_INTERNAL_OOXML_TEXT_ELEMENT_H
#define ODR_INTERNAL_OOXML_TEXT_ELEMENT_H

#include <internal/abstract/document.h>
#include <internal/common/document_element.h>
#include <internal/common/style.h>
#include <pugixml.hpp>
#include <string>

namespace odr::internal::ooxml::text {
class Document;
class StyleRegistry;
class Style;

class Element : public common::Element<Element> {
public:
  static abstract::Element *
  construct_default_element(pugi::xml_node node,
                            const abstract::Allocator *allocator);

  explicit Element(pugi::xml_node node);

  virtual common::ResolvedStyle
  partial_style(const abstract::Document *document) const;
  virtual common::ResolvedStyle
  intermediate_style(const abstract::Document *document,
                     const abstract::DocumentCursor *cursor) const;

protected:
  static const Document *document_(const abstract::Document *document);
  static const StyleRegistry *style_(const abstract::Document *document);
  static const std::unordered_map<std::string, std::string> &
  document_relations_(const abstract::Document *document);

  friend class Style;
};

} // namespace odr::internal::ooxml::text

#endif // ODR_INTERNAL_OOXML_TEXT_ELEMENT_H
