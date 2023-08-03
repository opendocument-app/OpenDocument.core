#ifndef ODR_INTERNAL_ODF_ELEMENT_H
#define ODR_INTERNAL_ODF_ELEMENT_H

#include <odr/internal/abstract/document.hpp>
#include <odr/internal/common/document_element.hpp>
#include <odr/internal/common/style.hpp>

#include <pugixml.hpp>

namespace odr::internal::odf {
class StyleRegistry;
} // namespace odr::internal::odf

namespace odr::internal::odf {
class Document;

class Element : public common::Element<Element> {
public:
  static std::unique_ptr<abstract::Element>
  construct_default_element(pugi::xml_node node);

  Element();
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
