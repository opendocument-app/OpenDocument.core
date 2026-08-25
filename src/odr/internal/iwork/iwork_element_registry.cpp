#include <odr/internal/iwork/iwork_element_registry.hpp>

#include <stdexcept>
#include <utility>

namespace odr::internal::iwork {

void ElementRegistry::clear() noexcept {
  m_elements.clear();
  m_texts.clear();
  m_frames.clear();
  m_slides.clear();
  m_tables.clear();
  m_sheets.clear();
  m_cells.clear();
}

ElementIdentifier ElementRegistry::Sheet::cell(const std::uint32_t column,
                                               const std::uint32_t row) const {
  const auto it = cells.find({row, column});
  return it == cells.end() ? null_element_id : it->second;
}

[[nodiscard]] std::size_t ElementRegistry::size() const noexcept {
  return m_elements.size();
}

std::tuple<ElementIdentifier, ElementRegistry::Element &>
ElementRegistry::create_element(const ElementType type) {
  Element &element = m_elements.emplace_back();
  ElementIdentifier element_id = m_elements.size();
  element.type = type;
  return {element_id, element};
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::Text &>
ElementRegistry::create_text_element() {
  const auto &[element_id, element] = create_element(ElementType::text);
  auto [it, success] = m_texts.emplace(element_id, Text{});
  return {element_id, element, it->second};
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::Frame &>
ElementRegistry::create_frame_element() {
  const auto &[element_id, element] = create_element(ElementType::frame);
  auto [it, success] = m_frames.emplace(element_id, Frame{});
  return {element_id, element, it->second};
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::Slide &>
ElementRegistry::create_slide_element() {
  const auto &[element_id, element] = create_element(ElementType::slide);
  auto [it, success] = m_slides.emplace(element_id, Slide{});
  return {element_id, element, it->second};
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::Table &>
ElementRegistry::create_table_element() {
  const auto &[element_id, element] = create_element(ElementType::table);
  auto [it, success] = m_tables.emplace(element_id, Table{});
  return {element_id, element, it->second};
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::Sheet &>
ElementRegistry::create_sheet_element() {
  const auto &[element_id, element] = create_element(ElementType::sheet);
  auto [it, success] = m_sheets.emplace(element_id, Sheet{});
  return {element_id, element, it->second};
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::Cell &>
ElementRegistry::create_cell_element(const ElementType type) {
  const auto &[element_id, element] = create_element(type);
  auto [it, success] = m_cells.emplace(element_id, Cell{});
  return {element_id, element, it->second};
}

ElementRegistry::Element &
ElementRegistry::element_at(const ElementIdentifier id) {
  check_element_id(id);
  return m_elements.at(id - 1);
}

ElementRegistry::Text &
ElementRegistry::text_element_at(const ElementIdentifier id) {
  check_text_id(id);
  return m_texts.at(id);
}

ElementRegistry::Frame &
ElementRegistry::frame_element_at(const ElementIdentifier id) {
  check_frame_id(id);
  return m_frames.at(id);
}

ElementRegistry::Slide &
ElementRegistry::slide_element_at(const ElementIdentifier id) {
  check_slide_id(id);
  return m_slides.at(id);
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

ElementRegistry::Cell &
ElementRegistry::cell_element_at(const ElementIdentifier id) {
  check_cell_id(id);
  return m_cells.at(id);
}

const ElementRegistry::Element &
ElementRegistry::element_at(const ElementIdentifier id) const {
  check_element_id(id);
  return m_elements.at(id - 1);
}

const ElementRegistry::Text &
ElementRegistry::text_element_at(const ElementIdentifier id) const {
  check_text_id(id);
  return m_texts.at(id);
}

const ElementRegistry::Frame &
ElementRegistry::frame_element_at(const ElementIdentifier id) const {
  check_frame_id(id);
  return m_frames.at(id);
}

const ElementRegistry::Slide &
ElementRegistry::slide_element_at(const ElementIdentifier id) const {
  check_slide_id(id);
  return m_slides.at(id);
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

const ElementRegistry::Cell &
ElementRegistry::cell_element_at(const ElementIdentifier id) const {
  check_cell_id(id);
  return m_cells.at(id);
}

void ElementRegistry::append_child(const ElementIdentifier parent_id,
                                   const ElementIdentifier child_id) {
  check_element_id(parent_id);
  check_element_id(child_id);
  if (element_at(child_id).parent_id != null_element_id) {
    throw std::invalid_argument(
        "ElementRegistry::append_child: child already has a parent");
  }

  const ElementIdentifier previous_sibling_id =
      element_at(parent_id).last_child_id;

  element_at(child_id).parent_id = parent_id;
  element_at(child_id).previous_sibling_id = previous_sibling_id;

  if (element_at(parent_id).first_child_id == null_element_id) {
    element_at(parent_id).first_child_id = child_id;
  } else {
    element_at(previous_sibling_id).next_sibling_id = child_id;
  }
  element_at(parent_id).last_child_id = child_id;
}

void ElementRegistry::append_table_column(const ElementIdentifier table_id,
                                          const ElementIdentifier column_id) {
  check_table_id(table_id);
  check_element_id(column_id);
  if (element_at(column_id).parent_id != null_element_id) {
    throw std::invalid_argument(
        "ElementRegistry::append_table_column: column already has a parent");
  }

  Table &table = table_element_at(table_id);
  const ElementIdentifier previous_id = table.last_column_id;

  element_at(column_id).parent_id = table_id;
  element_at(column_id).previous_sibling_id = previous_id;
  if (previous_id == null_element_id) {
    table.first_column_id = column_id;
  } else {
    element_at(previous_id).next_sibling_id = column_id;
  }
  table.last_column_id = column_id;
}

void ElementRegistry::append_sheet_cell(const ElementIdentifier sheet_id,
                                        const ElementIdentifier cell_id) {
  check_sheet_id(sheet_id);
  check_cell_id(cell_id);
  if (element_at(cell_id).parent_id != null_element_id) {
    throw std::invalid_argument(
        "ElementRegistry::append_sheet_cell: cell already has a parent");
  }

  const Cell &cell = cell_element_at(cell_id);
  element_at(cell_id).parent_id = sheet_id;
  sheet_element_at(sheet_id).cells.emplace(std::pair(cell.row, cell.column),
                                           cell_id);
}

void ElementRegistry::check_element_id(const ElementIdentifier id) const {
  if (id == null_element_id) {
    throw std::out_of_range("ElementRegistry::check_id: null identifier");
  }
  if (id - 1 >= m_elements.size()) {
    throw std::out_of_range(
        "ElementRegistry::check_id: identifier out of range");
  }
}

void ElementRegistry::check_text_id(const ElementIdentifier id) const {
  check_element_id(id);
  if (!m_texts.contains(id)) {
    throw std::out_of_range("ElementRegistry::check_id: identifier not found");
  }
}

void ElementRegistry::check_frame_id(const ElementIdentifier id) const {
  check_element_id(id);
  if (!m_frames.contains(id)) {
    throw std::out_of_range("ElementRegistry::check_id: identifier not found");
  }
}

void ElementRegistry::check_slide_id(const ElementIdentifier id) const {
  check_element_id(id);
  if (!m_slides.contains(id)) {
    throw std::out_of_range("ElementRegistry::check_id: identifier not found");
  }
}

void ElementRegistry::check_table_id(const ElementIdentifier id) const {
  check_element_id(id);
  if (!m_tables.contains(id)) {
    throw std::out_of_range("ElementRegistry::check_id: identifier not found");
  }
}

void ElementRegistry::check_sheet_id(const ElementIdentifier id) const {
  check_element_id(id);
  if (!m_sheets.contains(id)) {
    throw std::out_of_range("ElementRegistry::check_id: identifier not found");
  }
}

void ElementRegistry::check_cell_id(const ElementIdentifier id) const {
  check_element_id(id);
  if (!m_cells.contains(id)) {
    throw std::out_of_range("ElementRegistry::check_id: identifier not found");
  }
}

} // namespace odr::internal::iwork
