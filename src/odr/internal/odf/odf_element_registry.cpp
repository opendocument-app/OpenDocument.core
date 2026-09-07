#include <odr/internal/odf/odf_element_registry.hpp>

#include <algorithm>
#include <stdexcept>

namespace odr::internal::odf {

std::tuple<ElementIdentifier, ElementRegistry::Element &>
ElementRegistry::create_element(const ElementType type,
                                const pugi::xml_node node) {
  const auto &[element_id, element] = create_element_(type);
  element.node = node;
  return {element_id, element};
}

std::tuple<ElementIdentifier, ElementRegistry::Element &>
ElementRegistry::create_shape_element(const ShapeType shape_type,
                                      const pugi::xml_node node) {
  const auto &[element_id, element] = create_element(ElementType::frame, node);
  m_shape_types.emplace(element_id, shape_type);
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
  sheet.ordinal = static_cast<std::uint32_t>(m_sheet_ids.size());
  m_sheet_ids.push_back(static_cast<StoredId>(element_id));
  return {element_id, element, sheet};
}

ElementIdentifier
ElementRegistry::sheet_cell_id(const ElementIdentifier sheet_id,
                               const std::uint32_t column,
                               const std::uint32_t row) const {
  const Sheet &sheet = sheet_element_at(sheet_id);
  const Sheet::Cell *cell = sheet.cell(column, row);
  if (cell == nullptr || cell->element_id == null_element_id) {
    return null_element_id;
  }
  if (!m_sheet_cells.at(cell->element_id).is_repeated) {
    return cell->element_id;
  }
  const ElementIdentifier id = positional_id::make(sheet.ordinal, column, row);
  return id != null_element_id ? id : cell->element_id;
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

void ElementRegistry::append_column(const ElementIdentifier table_id,
                                    const ElementIdentifier column_id) {
  Table &table = table_element_at(table_id);
  link_child(table_id, column_id, table.first_column_id, table.last_column_id);
}

void ElementRegistry::append_shape(const ElementIdentifier sheet_id,
                                   const ElementIdentifier shape_id) {
  Sheet &sheet = sheet_element_at(sheet_id);
  link_child(sheet_id, shape_id, sheet.first_shape_id, sheet.last_shape_id);
}

void ElementRegistry::append_sheet_cell(const ElementIdentifier sheet_id,
                                        const ElementIdentifier cell_id) {
  if (m_sheets.find(sheet_id) == nullptr) {
    throw std::out_of_range(
        "ElementRegistry::append_sheet_cell: not a sheet identifier");
  }

  Element &cell = element_at(cell_id);
  if (cell.parent_id != null_element_id) {
    throw std::invalid_argument(
        "ElementRegistry::append_sheet_cell: child already has a parent");
  }

  cell.parent_id = static_cast<StoredId>(sheet_id);
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

[[nodiscard]] ShapeType
ElementRegistry::shape_type(const ElementIdentifier id) const {
  const ShapeType *entry = m_shape_types.find(id);
  return entry != nullptr ? *entry : ShapeType::none;
}

void ElementRegistry::set_list_type(const ElementIdentifier id,
                                    const ListType type) {
  check_element_id(id);
  m_list_types.emplace(id, type);
}

void ElementRegistry::set_list_marker(const ElementIdentifier id,
                                      ListMarker marker) {
  check_element_id(id);
  m_list_markers.emplace(id, std::move(marker));
}

[[nodiscard]] ListType
ElementRegistry::list_type(const ElementIdentifier id) const {
  const ListType *type = m_list_types.find(id);
  return type != nullptr ? *type : ListType::unordered;
}

[[nodiscard]] const ListMarker &
ElementRegistry::list_marker(const ElementIdentifier id) const {
  static const ListMarker none;
  const ListMarker *marker = m_list_markers.find(id);
  return marker != nullptr ? *marker : none;
}

} // namespace odr::internal::odf
