#ifndef ODR_INTERNAL_ODF_ELEMENT_H
#define ODR_INTERNAL_ODF_ELEMENT_H

#include <internal/abstract/document.h>
#include <internal/common/document_element.h>
#include <internal/common/style.h>
#include <pugixml.hpp>

namespace odr::internal::odf {
class StyleRegistry;
} // namespace odr::internal::odf

namespace odr::internal::odf {
class Document;

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
  virtual const char *style_name_(const abstract::Document *document) const;

  static const Document *document_(const abstract::Document *document);
  static const StyleRegistry *style_(const abstract::Document *document);
};

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_ELEMENT_H
