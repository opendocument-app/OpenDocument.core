#include <odr/internal/markdown/markdown_element_registry.hpp>

namespace odr::internal::markdown {

std::tuple<ElementIdentifier, ElementRegistry::Element &>
ElementRegistry::create_element(const ElementType type) {
  return create_element_(type);
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::Text &>
ElementRegistry::create_text_element() {
  const auto &[element_id, element] = create_element_(ElementType::text);
  Text &text = m_texts.emplace(element_id, Text{});
  return {element_id, element, text};
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::Link &>
ElementRegistry::create_link_element() {
  const auto &[element_id, element] = create_element_(ElementType::link);
  Link &link = m_links.emplace(element_id, Link{});
  return {element_id, element, link};
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::List &>
ElementRegistry::create_list_element() {
  const auto &[element_id, element] = create_element_(ElementType::list);
  List &list = m_lists.emplace(element_id, List{});
  return {element_id, element, list};
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::ListItem &>
ElementRegistry::create_list_item_element() {
  const auto &[element_id, element] = create_element_(ElementType::list_item);
  ListItem &list_item = m_list_items.emplace(element_id, ListItem{});
  return {element_id, element, list_item};
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::Table &>
ElementRegistry::create_table_element() {
  const auto &[element_id, element] = create_element_(ElementType::table);
  Table &table = m_tables.emplace(element_id, Table{});
  return {element_id, element, table};
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::TableCell &>
ElementRegistry::create_table_cell_element() {
  const auto &[element_id, element] = create_element_(ElementType::table_cell);
  TableCell &table_cell = m_table_cells.emplace(element_id, TableCell{});
  return {element_id, element, table_cell};
}

void ElementRegistry::append_column(const ElementIdentifier table_id,
                                    const ElementIdentifier column_id) {
  Table &table = table_element_at(table_id);
  link_child(table_id, column_id, table.first_column_id, table.last_column_id);
}

void ElementRegistry::set_element_text_style_index(const ElementIdentifier id,
                                                   const std::uint32_t index) {
  check_element_id(id);
  m_text_style_indices.emplace(id, index);
}

std::uint32_t
ElementRegistry::element_text_style_index(const ElementIdentifier id) const {
  const std::uint32_t *index = m_text_style_indices.find(id);
  return index != nullptr ? *index : 0;
}

void ElementRegistry::set_element_paragraph_style_index(
    const ElementIdentifier id, const std::uint32_t index) {
  check_element_id(id);
  m_paragraph_style_indices.emplace(id, index);
}

std::uint32_t ElementRegistry::element_paragraph_style_index(
    const ElementIdentifier id) const {
  const std::uint32_t *index = m_paragraph_style_indices.find(id);
  return index != nullptr ? *index : 0;
}

} // namespace odr::internal::markdown
