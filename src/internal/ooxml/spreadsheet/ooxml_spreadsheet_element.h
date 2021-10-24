#ifndef ODR_INTERNAL_OOXML_SPREADSHEET_ELEMENT_H
#define ODR_INTERNAL_OOXML_SPREADSHEET_ELEMENT_H

#include <internal/abstract/document.h>
#include <internal/common/document_element.h>
#include <internal/common/style.h>
#include <pugixml.hpp>
#include <string>

namespace odr::internal::ooxml::spreadsheet {
class Document;

class Element : public common::Element<Element> {
public:
  static abstract::Element *
  construct_default_element(pugi::xml_node node,
                            const abstract::Allocator &allocator);

  explicit Element(pugi::xml_node node);

  common::ResolvedStyle partial_style(const abstract::Document *document) const;
  common::ResolvedStyle
  intermediate_style(const abstract::Document *document,
                     const abstract::DocumentCursor *cursor) const;

protected:
  static const Document *document_(const abstract::Document *document);
  static pugi::xml_node sheet_(const abstract::Document *document,
                               const std::string &id);
};

} // namespace odr::internal::ooxml::spreadsheet

#endif // ODR_INTERNAL_OOXML_SPREADSHEET_ELEMENT_H
