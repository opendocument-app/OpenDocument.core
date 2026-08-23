#include <odr/internal/markdown/markdown_element_registry.hpp>

#include <stdexcept>

namespace odr::internal::markdown {

namespace {

/// Looks a payload up, turning a missing entry into an out-of-range error
/// rather than the map's default-constructed one.
template <typename Map> auto &payload_at(Map &map, const ElementIdentifier id) {
  const auto it = map.find(id);
  if (it == map.end()) {
    throw std::out_of_range(
        "markdown::ElementRegistry: element has no payload of that kind");
  }
  return it->second;
}

} // namespace

std::size_t ElementRegistry::size() const noexcept { return m_elements.size(); }

std::tuple<ElementIdentifier, ElementRegistry::Element &>
ElementRegistry::create_element(const ElementType type) {
  Element &element = m_elements.emplace_back();
  const ElementIdentifier element_id = m_elements.size();
  element.type = type;
  return {element_id, element};
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::Text &>
ElementRegistry::create_text_element() {
  const auto &[element_id, element] = create_element(ElementType::text);
  const auto [it, inserted] = m_texts.emplace(element_id, Text{});
  return {element_id, element, it->second};
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::Link &>
ElementRegistry::create_link_element() {
  const auto &[element_id, element] = create_element(ElementType::link);
  const auto [it, inserted] = m_links.emplace(element_id, Link{});
  return {element_id, element, it->second};
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::List &>
ElementRegistry::create_list_element() {
  const auto &[element_id, element] = create_element(ElementType::list);
  const auto [it, inserted] = m_lists.emplace(element_id, List{});
  return {element_id, element, it->second};
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::ListItem &>
ElementRegistry::create_list_item_element() {
  const auto &[element_id, element] = create_element(ElementType::list_item);
  const auto [it, inserted] = m_list_items.emplace(element_id, ListItem{});
  return {element_id, element, it->second};
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::Table &>
ElementRegistry::create_table_element() {
  const auto &[element_id, element] = create_element(ElementType::table);
  const auto [it, inserted] = m_tables.emplace(element_id, Table{});
  return {element_id, element, it->second};
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::TableCell &>
ElementRegistry::create_table_cell_element() {
  const auto &[element_id, element] = create_element(ElementType::table_cell);
  const auto [it, inserted] = m_table_cells.emplace(element_id, TableCell{});
  return {element_id, element, it->second};
}

ElementRegistry::Element &
ElementRegistry::element_at(const ElementIdentifier id) {
  check_element_id(id);
  return m_elements.at(id - 1);
}

ElementRegistry::Text &
ElementRegistry::text_element_at(const ElementIdentifier id) {
  return payload_at(m_texts, id);
}

ElementRegistry::Table &
ElementRegistry::table_element_at(const ElementIdentifier id) {
  return payload_at(m_tables, id);
}

const ElementRegistry::Element &
ElementRegistry::element_at(const ElementIdentifier id) const {
  check_element_id(id);
  return m_elements.at(id - 1);
}

const ElementRegistry::Text &
ElementRegistry::text_element_at(const ElementIdentifier id) const {
  return payload_at(m_texts, id);
}

const ElementRegistry::Link &
ElementRegistry::link_element_at(const ElementIdentifier id) const {
  return payload_at(m_links, id);
}

const ElementRegistry::List &
ElementRegistry::list_element_at(const ElementIdentifier id) const {
  return payload_at(m_lists, id);
}

const ElementRegistry::ListItem &
ElementRegistry::list_item_element_at(const ElementIdentifier id) const {
  return payload_at(m_list_items, id);
}

const ElementRegistry::Table &
ElementRegistry::table_element_at(const ElementIdentifier id) const {
  return payload_at(m_tables, id);
}

const ElementRegistry::TableCell &
ElementRegistry::table_cell_element_at(const ElementIdentifier id) const {
  return payload_at(m_table_cells, id);
}

void ElementRegistry::append_child(const ElementIdentifier parent_id,
                                   const ElementIdentifier child_id) {
  check_element_id(parent_id);
  check_element_id(child_id);
  if (element_at(child_id).parent_id != null_element_id) {
    throw std::invalid_argument(
        "markdown::ElementRegistry::append_child: child already has a parent");
  }

  Element &parent = element_at(parent_id);
  link_child(parent_id, child_id, parent.first_child_id, parent.last_child_id);
}

void ElementRegistry::append_column(const ElementIdentifier table_id,
                                    const ElementIdentifier column_id) {
  check_element_id(column_id);
  if (element_at(column_id).parent_id != null_element_id) {
    throw std::invalid_argument(
        "markdown::ElementRegistry::append_column: child already has a parent");
  }

  Table &table = table_element_at(table_id);
  link_child(table_id, column_id, table.first_column_id, table.last_column_id);
}

void ElementRegistry::link_child(const ElementIdentifier parent_id,
                                 const ElementIdentifier child_id,
                                 ElementIdentifier &first_id,
                                 ElementIdentifier &last_id) {
  Element &child = element_at(child_id);
  child.parent_id = parent_id;
  child.previous_sibling_id = last_id;

  if (first_id == null_element_id) {
    first_id = child_id;
  } else {
    element_at(last_id).next_sibling_id = child_id;
  }
  last_id = child_id;
}

void ElementRegistry::set_element_text_style_index(const ElementIdentifier id,
                                                   const std::uint32_t index) {
  check_element_id(id);
  m_text_style_indices[id] = index;
}

std::uint32_t
ElementRegistry::element_text_style_index(const ElementIdentifier id) const {
  const auto it = m_text_style_indices.find(id);
  return it != m_text_style_indices.end() ? it->second : 0;
}

void ElementRegistry::set_element_paragraph_style_index(
    const ElementIdentifier id, const std::uint32_t index) {
  check_element_id(id);
  m_paragraph_style_indices[id] = index;
}

std::uint32_t ElementRegistry::element_paragraph_style_index(
    const ElementIdentifier id) const {
  const auto it = m_paragraph_style_indices.find(id);
  return it != m_paragraph_style_indices.end() ? it->second : 0;
}

void ElementRegistry::check_element_id(const ElementIdentifier id) const {
  if (id == null_element_id) {
    throw std::out_of_range(
        "markdown::ElementRegistry::check_element_id: null identifier");
  }
  if (id - 1 >= m_elements.size()) {
    throw std::out_of_range(
        "markdown::ElementRegistry::check_element_id: identifier out of range");
  }
}

} // namespace odr::internal::markdown
