#ifndef ODR_INTERNAL_ODF_PARSER_HPP
#define ODR_INTERNAL_ODF_PARSER_HPP

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

template <typename element_t, typename... args_t>
std::tuple<element_t *, pugi::xml_node>
parse_element_tree(Document &document, pugi::xml_node node, args_t &&...args) {
  if (!node) {
    return std::make_tuple(nullptr, pugi::xml_node());
  }

  auto element_unique =
      std::make_unique<element_t>(node, std::forward<args_t>(args)...);
  auto element = element_unique.get();
  document.register_element_(std::move(element_unique));

  parse_element_children(document, element, node);

  return std::make_tuple(element, node.next_sibling());
}
template <>
std::tuple<Text *, pugi::xml_node>
parse_element_tree<Text>(Document &document, pugi::xml_node first);
template <>
std::tuple<Table *, pugi::xml_node>
parse_element_tree<Table>(Document &document, pugi::xml_node node);
template <>
std::tuple<TableRow *, pugi::xml_node>
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

#endif // ODR_INTERNAL_ODF_PARSER_HPP
