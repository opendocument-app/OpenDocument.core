#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_element_registry.hpp>

#include <odr/internal/util/map_util.hpp>

#include <stdexcept>

namespace odr::internal::ooxml::spreadsheet {

void ElementRegistry::clear() noexcept {
  m_elements.clear();
  m_texts.clear();
  m_sheets.clear();
}

[[nodiscard]] std::size_t ElementRegistry::size() const noexcept {
  return m_elements.size();
}

ExtendedElementIdentifier ElementRegistry::create_element() {
  m_elements.emplace_back();
  return ExtendedElementIdentifier(m_elements.size());
}

ElementRegistry::Text &
ElementRegistry::create_text_element(const ExtendedElementIdentifier id) {
  check_element_id(id);
  auto [it, success] = m_texts.emplace(id.element_id(), Text{});
  return it->second;
}

ElementRegistry::Sheet &
ElementRegistry::create_sheet_element(const ExtendedElementIdentifier id) {
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

void ElementRegistry::append_child(const ExtendedElementIdentifier parent_id,
                                   const ExtendedElementIdentifier child_id) {
  check_element_id(parent_id);
  check_element_id(child_id);

  const ExtendedElementIdentifier previous_sibling_id(
      element(parent_id).last_child_id);

  element(child_id).parent_id = parent_id;
  element(child_id).first_child_id = null_element_id;
  element(child_id).last_child_id = null_element_id;
  element(child_id).previous_sibling_id = previous_sibling_id.element_id();
  element(child_id).next_sibling_id = null_element_id;

  if (element(parent_id).first_child_id == null_element_id) {
    element(parent_id).first_child_id = child_id.element_id();
  } else {
    element(previous_sibling_id).next_sibling_id = child_id.element_id();
  }
  element(parent_id).last_child_id = child_id.element_id();
}

void ElementRegistry::append_shape(const ExtendedElementIdentifier sheet_id,
                                   const ExtendedElementIdentifier shape_id) {
  check_sheet_id(sheet_id);
  check_element_id(shape_id);

  const ExtendedElementIdentifier previous_sibling_id(
      sheet_element(sheet_id).last_shape_id);

  element(shape_id).parent_id = sheet_id;
  element(shape_id).first_child_id = null_element_id;
  element(shape_id).last_child_id = null_element_id;
  element(shape_id).previous_sibling_id = previous_sibling_id.element_id();
  element(shape_id).next_sibling_id = null_element_id;

  if (sheet_element(sheet_id).first_shape_id == null_element_id) {
    sheet_element(sheet_id).first_shape_id = shape_id.element_id();
  } else {
    element(previous_sibling_id).next_sibling_id = shape_id.element_id();
  }
  sheet_element(sheet_id).last_shape_id = shape_id.element_id();
}

void ElementRegistry::append_sheet_cell(
    const ExtendedElementIdentifier sheet_id,
    const ExtendedElementIdentifier cell_id) {
  check_sheet_id(sheet_id);
  check_element_id(cell_id);

  element(cell_id).parent_id = sheet_id;
  element(cell_id).first_child_id = null_element_id;
  element(cell_id).last_child_id = null_element_id;
  element(cell_id).previous_sibling_id = null_element_id;
  element(cell_id).next_sibling_id = null_element_id;
}

void ElementRegistry::Sheet::create_column(const std::uint32_t column_min,
                                           const std::uint32_t column_max,
                                           const pugi::xml_node element) {
  (void)column_min;
  columns[column_max] = {.node = element};
}

void ElementRegistry::Sheet::create_row(const std::uint32_t row,
                                        const pugi::xml_node element) {
  rows[row].node = element;
}

void ElementRegistry::Sheet::create_cell(const std::uint32_t column,
                                         const std::uint32_t row,
                                         const pugi::xml_node element) {
  Cell &cell = rows[row].cells[column];
  cell.node = element;
}

pugi::xml_node
ElementRegistry::Sheet::column(const std::uint32_t column) const {
  if (const auto it = util::map::lookup_greater_than(columns, column);
      it != std::end(columns)) {
    return it->second.node;
  }
  return {};
}

pugi::xml_node ElementRegistry::Sheet::row(const std::uint32_t row) const {
  if (const auto it = util::map::lookup_greater_than(rows, row);
      it != std::end(rows)) {
    return it->second.node;
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
      return cell_it->second.node;
    }
  }
  return {};
}

} // namespace odr::internal::ooxml::spreadsheet
