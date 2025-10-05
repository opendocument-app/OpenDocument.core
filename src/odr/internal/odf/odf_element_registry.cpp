#include <odr/internal/odf/odf_element_registry.hpp>

#include <odr/internal/util/map_util.hpp>

namespace odr::internal::odf {

void ElementRegistry::clear() noexcept { m_elements.clear(); }

[[nodiscard]] std::size_t ElementRegistry::size() const noexcept {
  return m_elements.size();
}

ExtendedElementIdentifier ElementRegistry::create_element() {
  m_elements.emplace_back();
  return ExtendedElementIdentifier(m_elements.size());
}

ElementRegistry::Table &
ElementRegistry::create_table_element(ExtendedElementIdentifier id) {
  check_element_id(id);
  auto [it, success] = m_tables.emplace(id.element_id(), Table{});
  return it->second;
}

ElementRegistry::Text &
ElementRegistry::create_text_element(ExtendedElementIdentifier id) {
  check_element_id(id);
  auto [it, success] = m_texts.emplace(id.element_id(), Text{});
  return it->second;
}

ElementRegistry::Sheet &
ElementRegistry::create_sheet_element(ExtendedElementIdentifier id) {
  check_element_id(id);
  auto [it, success] = m_sheets.emplace(id.element_id(), Sheet{});
  return it->second;
}

[[nodiscard]] ElementRegistry::Element &
ElementRegistry::element(const ExtendedElementIdentifier id) {
  check_element_id(id);
  return m_elements.at(id.element_id() - 1);
}

[[nodiscard]] const ElementRegistry::Element &
ElementRegistry::element(const ExtendedElementIdentifier id) const {
  check_element_id(id);
  return m_elements.at(id.element_id() - 1);
}

[[nodiscard]] ElementRegistry::Table &
ElementRegistry::table_element(const ExtendedElementIdentifier id) {
  check_table_id(id);
  return m_tables.at(id.element_id());
}

[[nodiscard]] const ElementRegistry::Table &
ElementRegistry::table_element(const ExtendedElementIdentifier id) const {
  check_table_id(id);
  return m_tables.at(id.element_id());
}

[[nodiscard]] ElementRegistry::Text &
ElementRegistry::text_element(const ExtendedElementIdentifier id) {
  check_text_id(id);
  return m_texts.at(id.element_id());
}

[[nodiscard]] const ElementRegistry::Text &
ElementRegistry::text_element(const ExtendedElementIdentifier id) const {
  check_text_id(id);
  return m_texts.at(id.element_id());
}

[[nodiscard]] ElementRegistry::Sheet &
ElementRegistry::sheet_element(const ExtendedElementIdentifier id) {
  check_element_id(id);
  if (!m_sheets.contains(id.element_id())) {
    throw std::out_of_range(
        "DocumentElementRegistry::check_id: identifier not found");
  }
  return m_sheets.at(id.element_id());
}

[[nodiscard]] const ElementRegistry::Sheet &
ElementRegistry::sheet_element(const ExtendedElementIdentifier id) const {
  check_element_id(id);
  if (!m_sheets.contains(id.element_id())) {
    throw std::out_of_range(
        "DocumentElementRegistry::check_id: identifier not found");
  }
  return m_sheets.at(id.element_id());
}

void ElementRegistry::check_element_id(
    const ExtendedElementIdentifier id) const {
  if (id.is_null()) {
    throw std::out_of_range(
        "DocumentElementRegistry::check_id: null identifier");
  }
  if (id.element_id() - 1 >= m_elements.size()) {
    throw std::out_of_range(
        "DocumentElementRegistry::check_id: identifier out of range");
  }
}

void ElementRegistry::check_table_id(const ExtendedElementIdentifier id) const {
  check_element_id(id);
  if (!m_tables.contains(id.element_id())) {
    throw std::out_of_range(
        "DocumentElementRegistry::check_id: identifier not found");
  }
}

void ElementRegistry::check_text_id(const ExtendedElementIdentifier id) const {
  check_element_id(id);
  if (!m_texts.contains(id.element_id())) {
    throw std::out_of_range(
        "DocumentElementRegistry::check_id: identifier not found");
  }
}

void ElementRegistry::check_sheet_id(const ExtendedElementIdentifier id) const {
  check_element_id(id);
  if (!m_sheets.contains(id.element_id())) {
    throw std::out_of_range(
        "DocumentElementRegistry::check_id: identifier not found");
  }
}

void ElementRegistry::Sheet::create_column(const std::uint32_t column,
                                           const std::uint32_t repeated,
                                           const pugi::xml_node element) {
  columns[column + repeated] = element;
}

void ElementRegistry::Sheet::create_row(const std::uint32_t row,
                                        const std::uint32_t repeated,
                                        const pugi::xml_node element) {
  rows[row + repeated].row = element;
}

void ElementRegistry::Sheet::create_cell(const std::uint32_t column,
                                         const std::uint32_t row,
                                         const std::uint32_t columns_repeated,
                                         const std::uint32_t rows_repeated,
                                         const pugi::xml_node element) {
  rows[row + rows_repeated].cells[column + columns_repeated] = element;
}

pugi::xml_node
ElementRegistry::Sheet::column(const std::uint32_t column) const {
  if (const auto it = util::map::lookup_greater_than(columns, column);
      it != std::end(columns)) {
    return it->second;
  }
  return {};
}

pugi::xml_node ElementRegistry::Sheet::row(const std::uint32_t row) const {
  if (const auto it = util::map::lookup_greater_than(rows, row);
      it != std::end(rows)) {
    return it->second.row;
  }
  return {};
}

pugi::xml_node ElementRegistry::Sheet::cell(const std::uint32_t column,
                                            const std::uint32_t row) const {
  if (const auto row_it = util::map::lookup_greater_than(rows, row);
      row_it != std::end(rows)) {
    const auto &cells = row_it->second.cells;
    if (const auto cell_it = util::map::lookup_greater_than(cells, column);
        cell_it != std::end(cells)) {
      return cell_it->second;
    }
  }
  return {};
}

} // namespace odr::internal::odf
