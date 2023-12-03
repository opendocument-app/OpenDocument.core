#ifndef ODR_INTERNAL_ODF_PARSER_H
#define ODR_INTERNAL_ODF_PARSER_H

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

Element *parse_tree(pugi::xml_node node,
                    std::vector<std::unique_ptr<Element>> &store);

std::tuple<Element *, std::vector<std::unique_ptr<Element>>>
parse_tree(pugi::xml_node node);

template <typename Derived>
std::tuple<Element *, pugi::xml_node>
parse_element_tree(pugi::xml_node node,
                   std::vector<std::unique_ptr<Element>> &store) {
  if (!node) {
    return std::make_tuple(nullptr, pugi::xml_node());
  }

  auto element_unique = std::make_unique<Derived>(node);
  auto element = element_unique.get();
  store.push_back(std::move(element_unique));

  parse_element_children(element, node, store);

  return std::make_tuple(element, node.next_sibling());
}
template <>
std::tuple<Element *, pugi::xml_node>
parse_element_tree<Text>(pugi::xml_node first,
                         std::vector<std::unique_ptr<Element>> &store);
template <>
std::tuple<Element *, pugi::xml_node>
parse_element_tree<Table>(pugi::xml_node node,
                          std::vector<std::unique_ptr<Element>> &store);
template <>
std::tuple<Element *, pugi::xml_node>
parse_element_tree<TableRow>(pugi::xml_node node,
                             std::vector<std::unique_ptr<Element>> &store);

std::tuple<Element *, pugi::xml_node>
parse_any_element_tree(pugi::xml_node node,
                       std::vector<std::unique_ptr<Element>> &store);

void parse_element_children(Element *element, pugi::xml_node node,
                            std::vector<std::unique_ptr<Element>> &store);
void parse_element_children(PresentationRoot *element, pugi::xml_node node,
                            std::vector<std::unique_ptr<Element>> &store);
void parse_element_children(SpreadsheetRoot *element, pugi::xml_node node,
                            std::vector<std::unique_ptr<Element>> &store);
void parse_element_children(DrawingRoot *element, pugi::xml_node node,
                            std::vector<std::unique_ptr<Element>> &store);

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_PARSER_H
