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

std::tuple<ExtendedElementIdentifier, ElementRegistry::Element &>
ElementRegistry::create_element(const ElementType type,
                                const pugi::xml_node node) {
  Element &element = m_elements.emplace_back();
  ExtendedElementIdentifier element_id(m_elements.size());
  element.type = type;
  element.node = node;
  return {element_id, element};
}

std::tuple<ExtendedElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::Text &>
ElementRegistry::create_text_element(const pugi::xml_node first_node) {
  const auto &[element_id, element] =
      create_element(ElementType::text, first_node);
  auto [it, success] = m_texts.emplace(element_id.element_id(), Text{});
  return {element_id, element, it->second};
}

std::tuple<ExtendedElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::Table &>
ElementRegistry::create_table_element(const pugi::xml_node node) {
  const auto &[element_id, element] = create_element(ElementType::table, node);
  auto [it, success] = m_tables.emplace(element_id.element_id(), Table{});
  return {element_id, element, it->second};
}

std::tuple<ExtendedElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::Sheet &>
ElementRegistry::create_sheet_element(const pugi::xml_node node) {
  const auto &[element_id, element] = create_element(ElementType::table, node);
  auto [it, success] = m_sheets.emplace(element_id.element_id(), Sheet{});
  return {element_id, element, it->second};
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

void ElementRegistry::append_child(const ExtendedElementIdentifier parent_id,
                                   const ExtendedElementIdentifier child_id) {
  check_element_id(parent_id);
  check_element_id(child_id);
  if (!element(child_id).parent_id.is_null()) {
    throw std::invalid_argument(
        "DocumentElementRegistry::append_child: child already has a parent");
  }

  const ExtendedElementIdentifier previous_sibling_id(
      element(parent_id).last_child_id);

  element(child_id).parent_id = parent_id;

  if (element(parent_id).first_child_id == null_element_id) {
    element(parent_id).first_child_id = child_id.element_id();
  } else {
    element(previous_sibling_id).next_sibling_id = child_id.element_id();
  }
  element(parent_id).last_child_id = child_id.element_id();
}

void ElementRegistry::append_column(const ExtendedElementIdentifier table_id,
                                    const ExtendedElementIdentifier column_id) {
  check_table_id(table_id);
  check_element_id(column_id);
  if (!element(column_id).parent_id.is_null()) {
    throw std::invalid_argument(
        "DocumentElementRegistry::append_column: child already has a parent");
  }

  const ExtendedElementIdentifier previous_sibling_id(
      table_element(table_id).last_column_id);

  element(column_id).parent_id = table_id;

  if (table_element(table_id).first_column_id == null_element_id) {
    table_element(table_id).first_column_id = column_id.element_id();
  } else {
    element(previous_sibling_id).next_sibling_id = column_id.element_id();
  }
  table_element(table_id).last_column_id = column_id.element_id();
}

void ElementRegistry::append_shape(const ExtendedElementIdentifier sheet_id,
                                   const ExtendedElementIdentifier shape_id) {
  check_sheet_id(sheet_id);
  check_element_id(shape_id);
  if (!element(shape_id).parent_id.is_null()) {
    throw std::invalid_argument(
        "DocumentElementRegistry::append_shape: child already has a parent");
  }

  const ExtendedElementIdentifier previous_sibling_id(
      sheet_element(sheet_id).last_shape_id);

  element(shape_id).parent_id = sheet_id;

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
  if (!element(cell_id).parent_id.is_null()) {
    throw std::invalid_argument("DocumentElementRegistry::append_sheet_cell: "
                                "child already has a parent");
  }

  element(cell_id).parent_id = sheet_id;
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
  columns[column + repeated] = {.node = element};
}

void ElementRegistry::Sheet::create_row(const std::uint32_t row,
                                        const std::uint32_t repeated,
                                        const pugi::xml_node element) {
  rows[row + repeated].node = element;
}

void ElementRegistry::Sheet::create_cell(const std::uint32_t column,
                                         const std::uint32_t row,
                                         const std::uint32_t columns_repeated,
                                         const std::uint32_t rows_repeated,
                                         const pugi::xml_node element) {
  const bool is_repeated = columns_repeated > 1 || rows_repeated > 1;

  Cell &cell = rows[row + rows_repeated].cells[column + columns_repeated];
  cell.node = element;
  cell.is_repeated = is_repeated;
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

} // namespace odr::internal::odf
