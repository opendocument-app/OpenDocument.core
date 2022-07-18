#ifndef ODR_INTERNAL_ODF_ELEMENT_H
#define ODR_INTERNAL_ODF_ELEMENT_H

#include <memory>
#include <odr/internal/abstract/document.hpp>
#include <odr/internal/common/document_element.hpp>
#include <odr/internal/common/style.hpp>
#include <pugixml.hpp>
#include <vector>

namespace odr::internal::odf {
class Document;
class StyleRegistry;

class Element : public common::Element {
public:
  Element();
  explicit Element(pugi::xml_node node);

  virtual common::ResolvedStyle
  partial_style(const abstract::Document *document) const;
  virtual common::ResolvedStyle
  intermediate_style(const abstract::Document *document) const;

protected:
  virtual const char *style_name_(const abstract::Document *document) const;

  static const Document *document_(const abstract::Document *document);
  static const StyleRegistry *style_(const abstract::Document *document);
};

std::tuple<abstract::Element *, std::vector<std::unique_ptr<abstract::Element>>>
parse_tree(pugi::xml_node node);

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_ELEMENT_H
