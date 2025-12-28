#include <odr/internal/ooxml/presentation/ooxml_presentation_element_registry.hpp>

#include <stdexcept>

namespace odr::internal::ooxml::presentation {

void ElementRegistry::clear() noexcept {
  m_elements.clear();
  m_tables.clear();
  m_texts.clear();
}

[[nodiscard]] std::size_t ElementRegistry::size() const noexcept {
  return m_elements.size();
}

std::tuple<ElementIdentifier, ElementRegistry::Element &>
ElementRegistry::create_element(const ElementType type,
                                const pugi::xml_node node) {
  Element &element = m_elements.emplace_back();
  ElementIdentifier element_id = m_elements.size();
  element.type = type;
  element.node = node;
  return {element_id, element};
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::Table &>
ElementRegistry::create_table_element(const pugi::xml_node node) {
  const auto &[element_id, element] = create_element(ElementType::table, node);
  auto [it, success] = m_tables.emplace(element_id, Table{});
  return {element_id, element, it->second};
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::Text &>
ElementRegistry::create_text_element(const pugi::xml_node first_node,
                                     const pugi::xml_node last_node) {
  const auto &[element_id, element] =
      create_element(ElementType::text, first_node);
  auto [it, success] = m_texts.emplace(element_id, Text{last_node});
  return {element_id, element, it->second};
}

ElementRegistry::Element &
ElementRegistry::element_at(const ElementIdentifier id) {
  check_element_id(id);
  return m_elements.at(id - 1);
}

ElementRegistry::Table &
ElementRegistry::table_element_at(const ElementIdentifier id) {
  check_table_id(id);
  return m_tables.at(id);
}

const ElementRegistry::Element &
ElementRegistry::element_at(const ElementIdentifier id) const {
  check_element_id(id);
  return m_elements.at(id - 1);
}

const ElementRegistry::Table &
ElementRegistry::table_element_at(const ElementIdentifier id) const {
  check_table_id(id);
  return m_tables.at(id);
}

ElementRegistry::Element *ElementRegistry::element(const ElementIdentifier id) {
  if (id == null_element_id || id - 1 >= m_elements.size()) {
    return nullptr;
  }
  return &m_elements.at(id - 1);
}

ElementRegistry::Table *
ElementRegistry::table_element(const ElementIdentifier id) {
  if (const auto it = m_tables.find(id); it != m_tables.end()) {
    return &it->second;
  }
  return nullptr;
}

ElementRegistry::Text *
ElementRegistry::text_element(const ElementIdentifier id) {
  if (const auto it = m_texts.find(id); it != m_texts.end()) {
    return &it->second;
  }
  return nullptr;
}

const ElementRegistry::Element *
ElementRegistry::element(const ElementIdentifier id) const {
  if (id == null_element_id || id - 1 >= m_elements.size()) {
    return nullptr;
  }
  return &m_elements.at(id - 1);
}

const ElementRegistry::Table *
ElementRegistry::table_element(const ElementIdentifier id) const {
  if (const auto it = m_tables.find(id); it != m_tables.end()) {
    return &it->second;
  }
  return nullptr;
}

const ElementRegistry::Text *
ElementRegistry::text_element(const ElementIdentifier id) const {
  if (const auto it = m_texts.find(id); it != m_texts.end()) {
    return &it->second;
  }
  return nullptr;
}

void ElementRegistry::check_element_id(const ElementIdentifier id) const {
  if (id == null_element_id) {
    throw std::out_of_range(
        "DocumentElementRegistry::check_id: null identifier");
  }
  if (id - 1 >= m_elements.size()) {
    throw std::out_of_range(
        "DocumentElementRegistry::check_id: identifier out of range");
  }
}

void ElementRegistry::check_table_id(const ElementIdentifier id) const {
  check_element_id(id);
  if (!m_tables.contains(id)) {
    throw std::out_of_range(
        "DocumentElementRegistry::check_id: identifier not found");
  }
}

void ElementRegistry::check_text_id(const ElementIdentifier id) const {
  check_element_id(id);
  if (!m_texts.contains(id)) {
    throw std::out_of_range(
        "DocumentElementRegistry::check_id: identifier not found");
  }
}

void ElementRegistry::append_child(const ElementIdentifier parent_id,
                                   const ElementIdentifier child_id) {
  check_element_id(parent_id);
  check_element_id(child_id);

  const ElementIdentifier previous_sibling_id =
      element_at(parent_id).last_child_id;

  element_at(child_id).parent_id = parent_id;
  element_at(child_id).previous_sibling_id = previous_sibling_id;

  if (element_at(parent_id).first_child_id == null_element_id) {
    element_at(parent_id).first_child_id = child_id;
  } else {
    element_at(previous_sibling_id).next_sibling_id = child_id;
  }
  element_at(parent_id).last_child_id = child_id;
}

void ElementRegistry::append_column(const ElementIdentifier table_id,
                                    const ElementIdentifier column_id) {
  check_table_id(table_id);
  check_element_id(column_id);

  const ElementIdentifier previous_sibling_id =
      table_element_at(table_id).last_column_id;

  element_at(column_id).parent_id = table_id;
  element_at(column_id).previous_sibling_id = previous_sibling_id;

  if (table_element_at(table_id).first_column_id == null_element_id) {
    table_element_at(table_id).first_column_id = column_id;
  } else {
    element_at(previous_sibling_id).next_sibling_id = column_id;
  }
  table_element_at(table_id).last_column_id = column_id;
}

} // namespace odr::internal::ooxml::presentation
