#ifndef ODR_INTERNAL_OOXML_PRESENTATION_ELEMENT_H
#define ODR_INTERNAL_OOXML_PRESENTATION_ELEMENT_H

#include <odr/internal/abstract/document.hpp>
#include <odr/internal/common/document_element.hpp>
#include <odr/internal/common/style.hpp>
#include <pugixml.hpp>
#include <string>

namespace odr::internal::ooxml::presentation {
class Document;

class Element : public common::Element<Element> {
public:
  static std::unique_ptr<abstract::Element>
  construct_default_element(pugi::xml_node node);

  explicit Element(pugi::xml_node node);

  common::ResolvedStyle partial_style(const abstract::Document *document) const;
  common::ResolvedStyle
  intermediate_style(const abstract::Document *document,
                     const abstract::DocumentCursor *cursor) const;

protected:
  static const Document *document_(const abstract::Document *document);
  static pugi::xml_node slide_(const abstract::Document *document,
                               const std::string &id);
};

} // namespace odr::internal::ooxml::presentation

#endif // ODR_INTERNAL_OOXML_PRESENTATION_ELEMENT_H
