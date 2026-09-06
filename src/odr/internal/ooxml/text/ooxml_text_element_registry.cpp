#include <odr/internal/ooxml/text/ooxml_text_element_registry.hpp>

namespace odr::internal::ooxml::text {

std::tuple<ElementIdentifier, ElementRegistry::Element &>
ElementRegistry::create_element(const ElementType type,
                                const pugi::xml_node node) {
  const auto &[element_id, element] = create_element_(type);
  element.node = node;
  return {element_id, element};
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::Table &>
ElementRegistry::create_table_element(const pugi::xml_node node) {
  const auto &[element_id, element] = create_element(ElementType::table, node);
  Table &table = m_tables.emplace(element_id, Table{});
  return {element_id, element, table};
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::Text &>
ElementRegistry::create_text_element(const pugi::xml_node first_node,
                                     const pugi::xml_node last_node) {
  const auto &[element_id, element] =
      create_element(ElementType::text, first_node);
  Text &text = m_texts.emplace(element_id, Text{last_node});
  return {element_id, element, text};
}

void ElementRegistry::append_column(const ElementIdentifier table_id,
                                    const ElementIdentifier column_id) {
  Table &table = table_element_at(table_id);
  link_child(table_id, column_id, table.first_column_id, table.last_column_id);
}

void ElementRegistry::set_list_type(const ElementIdentifier id,
                                    const ListType type) {
  check_element_id(id);
  m_list_types.emplace(id, type);
}

void ElementRegistry::set_list_marker(const ElementIdentifier id,
                                      ListMarker marker) {
  check_element_id(id);
  m_list_markers.emplace(id, std::move(marker));
}

ListType ElementRegistry::list_type(const ElementIdentifier id) const {
  const ListType *type = m_list_types.find(id);
  return type != nullptr ? *type : ListType::unordered;
}

const ListMarker &
ElementRegistry::list_marker(const ElementIdentifier id) const {
  static const ListMarker none;
  const ListMarker *marker = m_list_markers.find(id);
  return marker != nullptr ? *marker : none;
}

} // namespace odr::internal::ooxml::text
