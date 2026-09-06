#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_element_registry.hpp>

#include <odr/internal/util/map_util.hpp>

#include <stdexcept>

namespace odr::internal::ooxml::spreadsheet {

std::tuple<ElementIdentifier, ElementRegistry::Element &>
ElementRegistry::create_element(const ElementType type,
                                const pugi::xml_node node) {
  const auto &[element_id, element] = create_element_(type);
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
           ElementRegistry::Sheet &>
ElementRegistry::create_sheet_element(const pugi::xml_node node) {
  const auto &[element_id, element] = create_element(ElementType::sheet, node);
  Sheet &sheet = m_sheets.emplace(element_id, Sheet{});
  return {element_id, element, sheet};
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::SheetCell &>
ElementRegistry::create_sheet_cell_element(const pugi::xml_node node,
                                           const TablePosition &position) {
  const auto &[element_id, element] =
      create_element(ElementType::sheet_cell, node);
  SheetCell &sheet_cell =
      m_sheet_cells.emplace(element_id, SheetCell{.position = position});
  return {element_id, element, sheet_cell};
}

ElementRegistry::ElementRelations &
ElementRegistry::attach_element_relations(const ElementIdentifier id,
                                          const Relations &relations,
                                          const AbsPath &origin) {
  check_element_id(id);
  if (m_element_relations.find(id) != nullptr) {
    throw std::runtime_error("ElementRegistry::attach_element_relations: "
                             "relations already attached");
  }
  return m_element_relations.emplace(
      id, ElementRelations{.relations = &relations, .origin = origin});
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
  element_at(cell_id).parent_id = sheet_id;
}

void ElementRegistry::Sheet::register_column(
    [[maybe_unused]] const std::uint32_t column_min,
    const std::uint32_t column_max, const pugi::xml_node element) {
  columns[column_max] = {.node = element};
}

void ElementRegistry::Sheet::register_row(const std::uint32_t row,
                                          const pugi::xml_node element) {
  rows[row].node = element;
}

void ElementRegistry::Sheet::register_cell(const std::uint32_t column,
                                           const std::uint32_t row,
                                           const pugi::xml_node element,
                                           const ElementIdentifier element_id) {
  Cell &cell = cells[TablePosition(column, row)];
  cell.node = element;
  cell.element_id = element_id;
}

const ElementRegistry::Sheet::Column *
ElementRegistry::Sheet::column(const std::uint32_t column) const {
  if (const auto it = util::map::lookup_greater_or_equals(columns, column);
      it != std::end(columns)) {
    return &it->second;
  }
  return nullptr;
}

const ElementRegistry::Sheet::Row *
ElementRegistry::Sheet::row(const std::uint32_t row) const {
  if (const auto it = rows.find(row); it != std::end(rows)) {
    return &it->second;
  }
  return nullptr;
}

const ElementRegistry::Sheet::Cell *
ElementRegistry::Sheet::cell(const std::uint32_t column,
                             const std::uint32_t row) const {
  if (const auto it = cells.find(TablePosition(column, row));
      it != std::end(cells)) {
    return &it->second;
  }
  return nullptr;
}

pugi::xml_node
ElementRegistry::Sheet::column_node(const std::uint32_t column) const {
  if (const Column *column_entry = this->column(column);
      column_entry != nullptr) {
    return column_entry->node;
  }
  return {};
}

pugi::xml_node ElementRegistry::Sheet::row_node(const std::uint32_t row) const {
  if (const Row *row_entry = this->row(row); row_entry != nullptr) {
    return row_entry->node;
  }
  return {};
}

pugi::xml_node
ElementRegistry::Sheet::cell_node(const std::uint32_t column,
                                  const std::uint32_t row) const {
  if (const Cell *cell_entry = this->cell(column, row); cell_entry != nullptr) {
    return cell_entry->node;
  }
  return {};
}

} // namespace odr::internal::ooxml::spreadsheet
