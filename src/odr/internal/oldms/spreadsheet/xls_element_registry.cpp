#include <odr/internal/oldms/spreadsheet/xls_element_registry.hpp>

#include <algorithm>

namespace odr::internal::oldms::spreadsheet {

ElementIdentifier ElementRegistry::Sheet::cell(const std::uint32_t column,
                                               const std::uint32_t row) const {
  if (const auto it = cells.find(TablePosition(column, row));
      it != cells.end()) {
    return it->second;
  }
  return null_element_id;
}

std::tuple<ElementIdentifier, ElementRegistry::Element &>
ElementRegistry::create_element(const ElementType type) {
  return create_element_(type);
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::Text &>
ElementRegistry::create_text_element() {
  const auto &[element_id, element] = create_element_(ElementType::text);
  Text &text = m_texts.emplace(element_id, Text{});
  return {element_id, element, text};
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::Sheet &>
ElementRegistry::create_sheet_element() {
  const auto &[element_id, element] = create_element_(ElementType::sheet);
  Sheet &sheet = m_sheets.emplace(element_id, Sheet{});
  return {element_id, element, sheet};
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::SheetCell &>
ElementRegistry::create_sheet_cell_element(const TablePosition &position) {
  const auto &[element_id, element] = create_element_(ElementType::sheet_cell);
  SheetCell &sheet_cell =
      m_sheet_cells.emplace(element_id, SheetCell{.position = position});
  return {element_id, element, sheet_cell};
}

void ElementRegistry::append_sheet_cell(const ElementIdentifier sheet_id,
                                        const ElementIdentifier cell_id) {
  Sheet &sheet = sheet_element_at(sheet_id);
  const SheetCell &cell = sheet_cell_element_at(cell_id);

  element_at(cell_id).parent_id = sheet_id;
  sheet.cells[cell.position] = cell_id;
  sheet.content.rows = std::max(sheet.content.rows, cell.position.row + 1);
  sheet.content.columns =
      std::max(sheet.content.columns, cell.position.column + 1);
}

} // namespace odr::internal::oldms::spreadsheet
