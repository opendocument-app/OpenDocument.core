#include <odr/internal/odf/odf_element_registry.hpp>

#include <odr/internal/util/map_util.hpp>

#include <stdexcept>

namespace odr::internal::odf {

void ElementRegistry::clear() noexcept {
  m_elements.clear();
  m_tables.clear();
  m_texts.clear();
  m_sheets.clear();
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
           ElementRegistry::Text &>
ElementRegistry::create_text_element(const pugi::xml_node first_node,
                                     const pugi::xml_node last_node) {
  const auto &[element_id, element] =
      create_element(ElementType::text, first_node);
  auto [it, success] = m_texts.emplace(element_id, Text{last_node});
  return {element_id, element, it->second};
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::Table &>
ElementRegistry::create_table_element(const pugi::xml_node node) {
  const auto &[element_id, element] = create_element(ElementType::table, node);
  auto [it, success] = m_tables.emplace(element_id, Table{});
  return {element_id, element, it->second};
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::Sheet &>
ElementRegistry::create_sheet_element(const pugi::xml_node node) {
  const auto &[element_id, element] = create_element(ElementType::sheet, node);
  auto [it, success] = m_sheets.emplace(element_id, Sheet{});
  return {element_id, element, it->second};
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::SheetCell &>
ElementRegistry::create_sheet_cell_element(const pugi::xml_node node,
                                           const TablePosition &position,
                                           bool is_repeated) {
  const auto &[element_id, element] =
      create_element(ElementType::sheet_cell, node);
  auto [it, success] = m_sheet_cells.emplace(
      element_id, SheetCell{.position = position, .is_repeated = is_repeated});
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

ElementRegistry::Sheet &
ElementRegistry::sheet_element_at(const ElementIdentifier id) {
  check_sheet_id(id);
  return m_sheets.at(id);
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

const ElementRegistry::Sheet &
ElementRegistry::sheet_element_at(const ElementIdentifier id) const {
  check_sheet_id(id);
  return m_sheets.at(id);
}

ElementRegistry::Element *ElementRegistry::element(const ElementIdentifier id) {
  if (id == null_element_id || id - 1 >= m_elements.size()) {
    return nullptr;
  }
  return &m_elements.at(id - 1);
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

const ElementRegistry::Sheet *
ElementRegistry::sheet_element(const ElementIdentifier id) const {
  if (const auto it = m_sheets.find(id); it != m_sheets.end()) {
    return &it->second;
  }
  return nullptr;
}

const ElementRegistry::SheetCell *
ElementRegistry::sheet_cell_element(const ElementIdentifier id) const {
  if (const auto it = m_sheet_cells.find(id); it != m_sheet_cells.end()) {
    return &it->second;
  }
  return nullptr;
}

void ElementRegistry::append_child(const ElementIdentifier parent_id,
                                   const ElementIdentifier child_id) {
  check_element_id(parent_id);
  check_element_id(child_id);
  if (element_at(child_id).parent_id != null_element_id) {
    throw std::invalid_argument(
        "DocumentElementRegistry::append_child: child already has a parent");
  }

  const ElementIdentifier previous_sibling_id =
      element_at(parent_id).last_child_id;

  element_at(child_id).parent_id = parent_id;

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
  if (element_at(column_id).parent_id != null_element_id) {
    throw std::invalid_argument(
        "DocumentElementRegistry::append_column: child already has a parent");
  }

  const ElementIdentifier previous_sibling_id =
      table_element_at(table_id).last_column_id;

  element_at(column_id).parent_id = table_id;

  if (table_element_at(table_id).first_column_id == null_element_id) {
    table_element_at(table_id).first_column_id = column_id;
  } else {
    element_at(previous_sibling_id).next_sibling_id = column_id;
  }
  table_element_at(table_id).last_column_id = column_id;
}

void ElementRegistry::append_shape(const ElementIdentifier sheet_id,
                                   const ElementIdentifier shape_id) {
  check_sheet_id(sheet_id);
  check_element_id(shape_id);
  if (element_at(shape_id).parent_id != null_element_id) {
    throw std::invalid_argument(
        "DocumentElementRegistry::append_shape: child already has a parent");
  }

  const ElementIdentifier previous_sibling_id =
      sheet_element_at(sheet_id).last_shape_id;

  element_at(shape_id).parent_id = sheet_id;

  if (sheet_element_at(sheet_id).first_shape_id == null_element_id) {
    sheet_element_at(sheet_id).first_shape_id = shape_id;
  } else {
    element_at(previous_sibling_id).next_sibling_id = shape_id;
  }
  sheet_element_at(sheet_id).last_shape_id = shape_id;
}

void ElementRegistry::append_sheet_cell(const ElementIdentifier sheet_id,
                                        const ElementIdentifier cell_id) {
  check_sheet_id(sheet_id);
  check_element_id(cell_id);
  if (element_at(cell_id).parent_id != null_element_id) {
    throw std::invalid_argument("DocumentElementRegistry::append_sheet_cell: "
                                "child already has a parent");
  }

  element_at(cell_id).parent_id = sheet_id;
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

void ElementRegistry::check_sheet_id(const ElementIdentifier id) const {
  check_element_id(id);
  if (!m_sheets.contains(id)) {
    throw std::out_of_range(
        "DocumentElementRegistry::check_id: identifier not found");
  }
}

void ElementRegistry::check_sheet_cell_id(const ElementIdentifier id) const {
  check_element_id(id);
  if (!m_sheet_cells.contains(id)) {
    throw std::out_of_range(
        "DocumentElementRegistry::check_id: identifier not found");
  }
}

void ElementRegistry::Sheet::register_column(const std::uint32_t column,
                                             const std::uint32_t repeated,
                                             const pugi::xml_node element) {
  columns[column + repeated] = {.node = element};
}

void ElementRegistry::Sheet::register_row(const std::uint32_t row,
                                          const std::uint32_t repeated,
                                          const pugi::xml_node element) {
  rows[row + repeated].node = element;
}

void ElementRegistry::Sheet::register_cell(const std::uint32_t column,
                                           const std::uint32_t row,
                                           const std::uint32_t columns_repeated,
                                           const std::uint32_t rows_repeated,
                                           const ElementIdentifier element_id) {
  Cell &cell = rows[row + rows_repeated].cells[column + columns_repeated];
  cell.element_id = element_id;
}

const ElementRegistry::Sheet::Column *
ElementRegistry::Sheet::column(const std::uint32_t column) const {
  if (const auto it = util::map::lookup_greater_than(columns, column);
      it != std::end(columns)) {
    return &it->second;
  }
  return nullptr;
}

const ElementRegistry::Sheet::Row *
ElementRegistry::Sheet::row(const std::uint32_t row) const {
  if (const auto it = util::map::lookup_greater_than(rows, row);
      it != std::end(rows)) {
    return &it->second;
  }
  return nullptr;
}

const ElementRegistry::Sheet::Cell *
ElementRegistry::Sheet::cell(const std::uint32_t column,
                             const std::uint32_t row) const {
  const Row *row_entry = this->row(row);
  if (row_entry == nullptr) {
    return nullptr;
  }
  const auto &cells = row_entry->cells;
  if (const auto cell_it = util::map::lookup_greater_than(cells, column);
      cell_it != std::end(cells)) {
    return &cell_it->second;
  }
  return nullptr;
}

[[nodiscard]] pugi::xml_node
ElementRegistry::Sheet::column_node(const std::uint32_t column) const {
  if (const Column *column_entry = this->column(column);
      column_entry != nullptr) {
    return column_entry->node;
  }
  return {};
}

[[nodiscard]] pugi::xml_node
ElementRegistry::Sheet::row_node(const std::uint32_t row) const {
  if (const Row *row_entry = this->row(row); row_entry != nullptr) {
    return row_entry->node;
  }
  return {};
}

} // namespace odr::internal::odf
