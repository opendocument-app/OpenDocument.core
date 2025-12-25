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

ElementIdentifier ElementRegistry::create_element() {
  m_elements.emplace_back();
  return m_elements.size();
}

ElementRegistry::Table &
ElementRegistry::create_table_element(const ElementIdentifier id) {
  check_element_id(id);
  auto [it, success] = m_tables.emplace(id, Table{});
  return it->second;
}

ElementRegistry::Text &
ElementRegistry::create_text_element(const ElementIdentifier id) {
  check_element_id(id);
  auto [it, success] = m_texts.emplace(id, Text{});
  return it->second;
}

[[nodiscard]] ElementRegistry::Element &
ElementRegistry::element(const ElementIdentifier id) {
  check_element_id(id);
  return m_elements.at(id - 1);
}

[[nodiscard]] const ElementRegistry::Element &
ElementRegistry::element(const ElementIdentifier id) const {
  check_element_id(id);
  return m_elements.at(id - 1);
}

[[nodiscard]] ElementRegistry::Table &
ElementRegistry::table_element(const ElementIdentifier id) {
  check_table_id(id);
  return m_tables.at(id);
}

[[nodiscard]] const ElementRegistry::Table &
ElementRegistry::table_element(const ElementIdentifier id) const {
  check_table_id(id);
  return m_tables.at(id);
}

[[nodiscard]] ElementRegistry::Text &
ElementRegistry::text_element(const ElementIdentifier id) {
  check_text_id(id);
  return m_texts.at(id);
}

[[nodiscard]] const ElementRegistry::Text &
ElementRegistry::text_element(const ElementIdentifier id) const {
  check_text_id(id);
  return m_texts.at(id);
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
      element(parent_id).last_child_id;

  element(child_id).parent_id = parent_id;

  if (element(parent_id).first_child_id == null_element_id) {
    element(parent_id).first_child_id = child_id;
  } else {
    element(previous_sibling_id).next_sibling_id = child_id;
  }
  element(parent_id).last_child_id = child_id;
}

void ElementRegistry::append_column(const ElementIdentifier table_id,
                                    const ElementIdentifier column_id) {
  check_table_id(table_id);
  check_element_id(column_id);

  const ElementIdentifier previous_sibling_id =
      table_element(table_id).last_column_id;

  element(column_id).parent_id = table_id;
  element(column_id).first_child_id = null_element_id;
  element(column_id).last_child_id = null_element_id;
  element(column_id).previous_sibling_id = previous_sibling_id;
  element(column_id).next_sibling_id = null_element_id;

  if (table_element(table_id).first_column_id == null_element_id) {
    table_element(table_id).first_column_id = column_id;
  } else {
    element(previous_sibling_id).next_sibling_id = column_id;
  }
  table_element(table_id).last_column_id = column_id;
}

} // namespace odr::internal::ooxml::presentation
