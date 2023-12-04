#ifndef ODR_INTERNAL_ODF_PARSER_H
#define ODR_INTERNAL_ODF_PARSER_H

#include <odr/internal/odf/odf_document.hpp>

#include <memory>
#include <utility>
#include <vector>

#include <pugixml.hpp>

namespace odr::internal::odf {
class Element;
class PresentationRoot;
class SpreadsheetRoot;
class DrawingRoot;
class Text;
class Table;
class TableRow;

Element *parse_tree(Document &document, pugi::xml_node node);

template <typename Derived>
std::tuple<Element *, pugi::xml_node> parse_element_tree(Document &document,
                                                         pugi::xml_node node) {
  if (!node) {
    return std::make_tuple(nullptr, pugi::xml_node());
  }

  auto element_unique = std::make_unique<Derived>(node);
  auto element = element_unique.get();
  document.register_element_(std::move(element_unique));

  parse_element_children(document, element, node);

  return std::make_tuple(element, node.next_sibling());
}
template <>
std::tuple<Element *, pugi::xml_node>
parse_element_tree<Text>(Document &document, pugi::xml_node first);
template <>
std::tuple<Element *, pugi::xml_node>
parse_element_tree<Table>(Document &document, pugi::xml_node node);
template <>
std::tuple<Element *, pugi::xml_node>
parse_element_tree<TableRow>(Document &document, pugi::xml_node node);

std::tuple<Element *, pugi::xml_node>
parse_any_element_tree(Document &document, pugi::xml_node node);

void parse_element_children(Document &document, Element *element,
                            pugi::xml_node node);
void parse_element_children(Document &document, PresentationRoot *element,
                            pugi::xml_node node);
void parse_element_children(Document &document, SpreadsheetRoot *element,
                            pugi::xml_node node);
void parse_element_children(Document &document, DrawingRoot *element,
                            pugi::xml_node node);

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_PARSER_H
