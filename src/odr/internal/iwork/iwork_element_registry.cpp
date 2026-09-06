#include <odr/internal/iwork/iwork_element_registry.hpp>

namespace odr::internal::iwork {

ElementIdentifier ElementRegistry::Sheet::cell(const std::uint32_t column,
                                               const std::uint32_t row) const {
  const auto it = cells.find({row, column});
  return it == cells.end() ? null_element_id : it->second;
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
           ElementRegistry::Frame &>
ElementRegistry::create_frame_element() {
  const auto &[element_id, element] = create_element_(ElementType::frame);
  Frame &frame = m_frames.emplace(element_id, Frame{});
  return {element_id, element, frame};
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::Slide &>
ElementRegistry::create_slide_element() {
  const auto &[element_id, element] = create_element_(ElementType::slide);
  Slide &slide = m_slides.emplace(element_id, Slide{});
  return {element_id, element, slide};
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::Table &>
ElementRegistry::create_table_element() {
  const auto &[element_id, element] = create_element_(ElementType::table);
  Table &table = m_tables.emplace(element_id, Table{});
  return {element_id, element, table};
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::Sheet &>
ElementRegistry::create_sheet_element() {
  const auto &[element_id, element] = create_element_(ElementType::sheet);
  Sheet &sheet = m_sheets.emplace(element_id, Sheet{});
  return {element_id, element, sheet};
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::Cell &>
ElementRegistry::create_cell_element(const ElementType type) {
  const auto &[element_id, element] = create_element_(type);
  Cell &cell = m_cells.emplace(element_id, Cell{});
  return {element_id, element, cell};
}

void ElementRegistry::append_table_column(const ElementIdentifier table_id,
                                          const ElementIdentifier column_id) {
  Table &table = table_element_at(table_id);
  link_child(table_id, column_id, table.first_column_id, table.last_column_id);
}

void ElementRegistry::append_sheet_cell(const ElementIdentifier sheet_id,
                                        const ElementIdentifier cell_id) {
  Sheet &sheet = sheet_element_at(sheet_id);
  const Cell &cell = cell_element_at(cell_id);

  Element &cell_element = element_at(cell_id);
  if (cell_element.parent_id != null_element_id) {
    throw std::invalid_argument(
        "ElementRegistry::append_sheet_cell: cell already has a parent");
  }

  cell_element.parent_id = sheet_id;
  sheet.cells.emplace(std::pair(cell.row, cell.column), cell_id);
}

} // namespace odr::internal::iwork
