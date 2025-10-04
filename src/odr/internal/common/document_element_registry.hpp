#pragma once

#include <odr/document_element.hpp>
#include <odr/document_element_identifier.hpp>

#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace odr::internal::common {

template <typename Derived> class ElementRegistry {
public:
  struct Entry {
    ExtendedElementIdentifier parent_id;
    ElementIdentifier first_child_id{};
    ElementIdentifier last_child_id{};
    ElementIdentifier previous_sibling_id{};
    ElementIdentifier next_sibling_id{};
    ElementType type{};
  };

  struct Table {
    ElementIdentifier first_column_id{};
    ElementIdentifier last_column_id{};
  };

  void clear() noexcept { m_entries.clear(); }

  [[nodiscard]] std::size_t size() const noexcept { return m_entries.size(); }

  ElementIdentifier create_element() {
    m_entries.emplace_back();
    return m_entries.size();
  }
  typename Derived::Table &create_table(ElementIdentifier id) {
    check_element_id(id);
    auto [it, success] = m_tables.emplace(id, Table{});
    return it->second;
  }

  [[nodiscard]] Derived::Entry &entry(ElementIdentifier id) {
    check_element_id(id);
    return m_entries.at(id.element_id() - 1);
  }
  [[nodiscard]] const Derived::Entry &entry(ElementIdentifier id) const {
    check_element_id(id);
    return m_entries.at(id.element_id() - 1);
  }

  [[nodiscard]] Table &table(ElementIdentifier id) {
    check_table_id(id);
    return m_tables.at(id);
  }
  [[nodiscard]] const Table &table(ElementIdentifier id) const {
    check_table_id(id);
    return m_tables.at(id);
  }

private:
  std::vector<typename Derived::Entry> m_entries;
  std::unordered_map<ElementIdentifier, typename Derived::Table> m_tables;

  void check_element_id(ElementIdentifier id) const {
    if (id.is_null()) {
      throw std::out_of_range(
          "DocumentElementRegistry::check_id: null identifier");
    }
    if (id.element_id() - 1 >= m_entries.size()) {
      throw std::out_of_range(
          "DocumentElementRegistry::check_id: identifier out of range");
    }
  }
  void check_table_id(ElementIdentifier id) const {
    check_element_id(id);
    if (!m_tables.contains(id)) {
      throw std::out_of_range(
          "DocumentElementRegistry::check_id: identifier not found");
    }
  }
};

} // namespace odr::internal::common
