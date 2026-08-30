#include <odr/internal/odf/odf_element_registry.hpp>

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace odr::internal::odf {

void ElementRegistry::clear() noexcept {
  m_elements.clear();
  m_texts.clear();
  m_tables.clear();
  m_sheets.clear();
  m_sheet_cells.clear();
  m_list_types.clear();
  m_list_markers.clear();
}

[[nodiscard]] std::size_t ElementRegistry::size() const noexcept {
  return m_elements.size();
}

std::tuple<ElementIdentifier, ElementRegistry::Element &>
ElementRegistry::create_element(const ElementType type,
                                const pugi::xml_node node) {
  if (m_elements.size() >= std::numeric_limits<StoredId>::max()) {
    throw std::overflow_error(
        "ElementRegistry::create_element: out of identifiers");
  }

  Element &element = m_elements.emplace_back();
  const ElementIdentifier element_id = m_elements.size();
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
  Text &text = m_texts.emplace(element_id, Text{last_node});
  return {element_id, element, text};
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::Table &>
ElementRegistry::create_table_element(const pugi::xml_node node) {
  const auto &[element_id, element] = create_element(ElementType::table, node);
  Table &table = m_tables.emplace(element_id, Table{});
  return {element_id, element, table};
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::Sheet &>
ElementRegistry::create_sheet_element(const pugi::xml_node node) {
  const auto &[element_id, element] = create_element(ElementType::sheet, node);
  Sheet &sheet = m_sheets.emplace(element_id, Sheet{});
  return {element_id, element, sheet};
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::SheetCell &>
ElementRegistry::create_sheet_cell_element(const pugi::xml_node node,
                                           const TablePosition &position,
                                           const bool is_repeated) {
  const auto &[element_id, element] =
      create_element(ElementType::sheet_cell, node);
  SheetCell &sheet_cell = m_sheet_cells.emplace(
      element_id, SheetCell{.position = position, .is_repeated = is_repeated});
  return {element_id, element, sheet_cell};
}

ElementRegistry::Element &
ElementRegistry::element_at(const ElementIdentifier id) {
  check_element_id(id);
  return m_elements.at(id - 1);
}

ElementRegistry::Text &
ElementRegistry::text_element_at(const ElementIdentifier id) {
  check_element_id(id);
  return m_texts.at(id);
}

ElementRegistry::Table &
ElementRegistry::table_element_at(const ElementIdentifier id) {
  check_element_id(id);
  return m_tables.at(id);
}

ElementRegistry::Sheet &
ElementRegistry::sheet_element_at(const ElementIdentifier id) {
  check_element_id(id);
  return m_sheets.at(id);
}

const ElementRegistry::Element &
ElementRegistry::element_at(const ElementIdentifier id) const {
  check_element_id(id);
  return m_elements.at(id - 1);
}

const ElementRegistry::Text &
ElementRegistry::text_element_at(const ElementIdentifier id) const {
  check_element_id(id);
  return m_texts.at(id);
}

const ElementRegistry::Table &
ElementRegistry::table_element_at(const ElementIdentifier id) const {
  check_element_id(id);
  return m_tables.at(id);
}

const ElementRegistry::Sheet &
ElementRegistry::sheet_element_at(const ElementIdentifier id) const {
  check_element_id(id);
  return m_sheets.at(id);
}

const ElementRegistry::SheetCell &
ElementRegistry::sheet_cell_element_at(const ElementIdentifier id) const {
  check_element_id(id);
  return m_sheet_cells.at(id);
}

const ElementRegistry::SheetCell *
ElementRegistry::sheet_cell_element(const ElementIdentifier id) const {
  return m_sheet_cells.find(id);
}

void ElementRegistry::link_child(const ElementIdentifier parent_id,
                                 const ElementIdentifier child_id,
                                 StoredId &first_id, StoredId &last_id) {
  Element &child = element_at(child_id);
  child.parent_id = static_cast<StoredId>(parent_id);
  child.previous_sibling_id = last_id;

  if (first_id == null_element_id) {
    first_id = static_cast<StoredId>(child_id);
  } else {
    element_at(last_id).next_sibling_id = static_cast<StoredId>(child_id);
  }
  last_id = static_cast<StoredId>(child_id);
}

void ElementRegistry::append_child(const ElementIdentifier parent_id,
                                   const ElementIdentifier child_id) {
  check_element_id(parent_id);
  check_element_id(child_id);
  if (element_at(child_id).parent_id != null_element_id) {
    throw std::invalid_argument(
        "DocumentElementRegistry::append_child: child already has a parent");
  }

  Element &parent = element_at(parent_id);
  link_child(parent_id, child_id, parent.first_child_id, parent.last_child_id);
}

void ElementRegistry::append_column(const ElementIdentifier table_id,
                                    const ElementIdentifier column_id) {
  Table &table = table_element_at(table_id);
  check_element_id(column_id);
  if (element_at(column_id).parent_id != null_element_id) {
    throw std::invalid_argument(
        "DocumentElementRegistry::append_column: child already has a parent");
  }

  link_child(table_id, column_id, table.first_column_id, table.last_column_id);
}

void ElementRegistry::append_shape(const ElementIdentifier sheet_id,
                                   const ElementIdentifier shape_id) {
  Sheet &sheet = sheet_element_at(sheet_id);
  check_element_id(shape_id);
  if (element_at(shape_id).parent_id != null_element_id) {
    throw std::invalid_argument(
        "DocumentElementRegistry::append_shape: child already has a parent");
  }

  link_child(sheet_id, shape_id, sheet.first_shape_id, sheet.last_shape_id);
}

void ElementRegistry::append_sheet_cell(const ElementIdentifier sheet_id,
                                        const ElementIdentifier cell_id) {
  check_sheet_id(sheet_id);
  check_element_id(cell_id);
  if (element_at(cell_id).parent_id != null_element_id) {
    throw std::invalid_argument("DocumentElementRegistry::append_sheet_cell: "
                                "child already has a parent");
  }

  element_at(cell_id).parent_id = static_cast<StoredId>(sheet_id);
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

void ElementRegistry::check_sheet_id(const ElementIdentifier id) const {
  check_element_id(id);
  if (m_sheets.find(id) == nullptr) {
    throw std::out_of_range(
        "DocumentElementRegistry::check_id: identifier not found");
  }
}

void ElementRegistry::Sheet::register_column(const std::uint32_t column,
                                             const std::uint32_t repeated,
                                             const pugi::xml_node element) {
  const std::uint32_t end = column + repeated;
  if (!columns.empty() && columns.back().end >= end) {
    columns.back() = {.end = end, .node = element};
    return;
  }
  columns.push_back({.end = end, .node = element});
}

void ElementRegistry::Sheet::register_row(const std::uint32_t row,
                                          const std::uint32_t repeated,
                                          const pugi::xml_node element) {
  const std::uint32_t end = row + repeated;
  if (!rows.empty() && rows.back().end >= end) {
    rows.back().end = end;
    rows.back().node = element;
    return;
  }
  rows.push_back({.end = end,
                  .first_cell = static_cast<std::uint32_t>(cells.size()),
                  .node = element});
}

void ElementRegistry::Sheet::register_cell(const std::uint32_t column,
                                           const std::uint32_t row,
                                           const std::uint32_t columns_repeated,
                                           const std::uint32_t rows_repeated,
                                           const pugi::xml_node element,
                                           const ElementIdentifier element_id) {
  const std::uint32_t row_end = row + rows_repeated;
  if (rows.empty() || rows.back().end != row_end) {
    throw std::invalid_argument(
        "ElementRegistry::Sheet::register_cell: no row to hold the cell");
  }

  const std::uint32_t end = column + columns_repeated;
  if (cells.size() > rows.back().first_cell && cells.back().end >= end) {
    cells.back() = {.end = end,
                    .element_id = static_cast<StoredId>(element_id),
                    .node = element};
    return;
  }
  cells.push_back({.end = end,
                   .element_id = static_cast<StoredId>(element_id),
                   .node = element});
}

namespace {

/// The entry whose range covers @p at, i.e. the first one ending past it.
template <typename Entry>
const Entry *lookup(const std::span<const Entry> entries,
                    const std::uint32_t at) {
  const auto it = std::ranges::upper_bound(entries, at, {}, &Entry::end);
  return it != std::end(entries) ? &*it : nullptr;
}

} // namespace

const ElementRegistry::Sheet::Column *
ElementRegistry::Sheet::column(const std::uint32_t column) const {
  return lookup<Column>(columns, column);
}

const ElementRegistry::Sheet::Row *
ElementRegistry::Sheet::row(const std::uint32_t row) const {
  return lookup<Row>(rows, row);
}

const ElementRegistry::Sheet::Cell *
ElementRegistry::Sheet::cell(const std::uint32_t column,
                             const std::uint32_t row) const {
  const Row *row_entry = this->row(row);
  return row_entry != nullptr ? lookup<Cell>(row_cells(*row_entry), column)
                              : nullptr;
}

std::span<const ElementRegistry::Sheet::Cell>
ElementRegistry::Sheet::row_cells(const Row &row) const {
  const auto next = &row + 1;
  const std::size_t end =
      next != rows.data() + rows.size() ? next->first_cell : cells.size();
  return {cells.data() + row.first_cell, end - row.first_cell};
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

[[nodiscard]] pugi::xml_node
ElementRegistry::Sheet::cell_node(const std::uint32_t column,
                                  const std::uint32_t row) const {
  if (const Cell *cell_entry = this->cell(column, row); cell_entry != nullptr) {
    return cell_entry->node;
  }
  return {};
}

void ElementRegistry::set_list_type(const ElementIdentifier id,
                                    const ListType type) {
  check_element_id(id);
  m_list_types[id] = type;
}

void ElementRegistry::set_list_marker(const ElementIdentifier id,
                                      ListMarker marker) {
  check_element_id(id);
  m_list_markers[id] = std::move(marker);
}

[[nodiscard]] ListType
ElementRegistry::list_type(const ElementIdentifier id) const {
  const auto it = m_list_types.find(id);
  return it != std::end(m_list_types) ? it->second : ListType::unordered;
}

[[nodiscard]] const ListMarker &
ElementRegistry::list_marker(const ElementIdentifier id) const {
  static const ListMarker none;
  const auto it = m_list_markers.find(id);
  return it != std::end(m_list_markers) ? it->second : none;
}

} // namespace odr::internal::odf
