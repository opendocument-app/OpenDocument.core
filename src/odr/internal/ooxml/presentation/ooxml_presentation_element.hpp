#ifndef ODR_INTERNAL_OOXML_PRESENTATION_ELEMENT_H
#define ODR_INTERNAL_OOXML_PRESENTATION_ELEMENT_H

#include <odr/internal/abstract/document.hpp>
#include <odr/internal/common/document_element.hpp>
#include <odr/internal/common/style.hpp>
#include <pugixml.hpp>
#include <string>

namespace odr::internal::ooxml::presentation {
class Document;

class Element : public common::Element {
public:
  explicit Element(pugi::xml_node node);

  common::ResolvedStyle partial_style(const abstract::Document *document) const;
  common::ResolvedStyle
  intermediate_style(const abstract::Document *document) const;

protected:
  static const Document *document_(const abstract::Document *document);
  static pugi::xml_node slide_(const abstract::Document *document,
                               const std::string &id);
};

std::tuple<abstract::Element *, std::vector<std::unique_ptr<abstract::Element>>>
parse_tree(pugi::xml_node node);

} // namespace odr::internal::ooxml::presentation

#endif // ODR_INTERNAL_OOXML_PRESENTATION_ELEMENT_H
