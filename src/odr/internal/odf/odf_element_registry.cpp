#include <odr/internal/odf/odf_element_registry.hpp>

namespace odr::internal::odf {

void ElementRegistry::clear() noexcept { m_entries.clear(); }

[[nodiscard]] std::size_t ElementRegistry::size() const noexcept {
  return m_entries.size();
}

ElementIdentifier ElementRegistry::create_element() {
  m_entries.emplace_back();
  return m_entries.size();
}

ElementRegistry::Table &ElementRegistry::create_table(ElementIdentifier id) {
  check_element_id(id);
  auto [it, success] = m_tables.emplace(id, Table{});
  return it->second;
}

[[nodiscard]] ElementRegistry::Entry &
ElementRegistry::entry(const ElementIdentifier id) {
  check_element_id(id);
  return m_entries.at(id - 1);
}

[[nodiscard]] const ElementRegistry::Entry &
ElementRegistry::entry(const ElementIdentifier id) const {
  check_element_id(id);
  return m_entries.at(id - 1);
}

[[nodiscard]] ElementRegistry::Table &
ElementRegistry::table(const ElementIdentifier id) {
  check_table_id(id);
  return m_tables.at(id);
}

[[nodiscard]] const ElementRegistry::Table &
ElementRegistry::table(const ElementIdentifier id) const {
  check_table_id(id);
  return m_tables.at(id);
}

[[nodiscard]] ElementRegistry::Text &
ElementRegistry::text(const ElementIdentifier id) {
  check_text_id(id);
  return m_texts.at(id);
}

[[nodiscard]] const ElementRegistry::Text &
ElementRegistry::text(const ElementIdentifier id) const {
  check_text_id(id);
  return m_texts.at(id);
}

void ElementRegistry::check_element_id(const ElementIdentifier id) const {
  if (id == null_element_id) {
    throw std::out_of_range(
        "DocumentElementRegistry::check_id: null identifier");
  }
  if (id - 1 >= m_entries.size()) {
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

} // namespace odr::internal::odf
