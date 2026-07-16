#include <odr/internal/oldms/spreadsheet/xls_element_registry.hpp>

#include <algorithm>
#include <stdexcept>

namespace odr::internal::oldms::spreadsheet {

ElementIdentifier ElementRegistry::Sheet::cell(const std::uint32_t column,
                                               const std::uint32_t row) const {
  if (const auto it = cells.find(TablePosition(column, row));
      it != cells.end()) {
    return it->second;
  }
  return null_element_id;
}

void ElementRegistry::clear() noexcept {
  m_elements.clear();
  m_texts.clear();
  m_sheets.clear();
  m_sheet_cells.clear();
  m_cell_styles.clear();
  m_font_names.clear();
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
           ElementRegistry::Sheet &>
ElementRegistry::create_sheet_element() {
  const auto &[element_id, element] = create_element(ElementType::sheet);
  auto [it, success] = m_sheets.emplace(element_id, Sheet{});
  return {element_id, element, it->second};
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::SheetCell &>
ElementRegistry::create_sheet_cell_element(const TablePosition &position) {
  const auto &[element_id, element] = create_element(ElementType::sheet_cell);
  auto [it, success] = m_sheet_cells.emplace(element_id, SheetCell{position});
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

ElementRegistry::Sheet &
ElementRegistry::sheet_element_at(const ElementIdentifier id) {
  check_sheet_id(id);
  return m_sheets.at(id);
}

ElementRegistry::SheetCell &
ElementRegistry::sheet_cell_element_at(const ElementIdentifier id) {
  check_sheet_cell_id(id);
  return m_sheet_cells.at(id);
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

const ElementRegistry::Sheet &
ElementRegistry::sheet_element_at(const ElementIdentifier id) const {
  check_sheet_id(id);
  return m_sheets.at(id);
}

const ElementRegistry::SheetCell &
ElementRegistry::sheet_cell_element_at(const ElementIdentifier id) const {
  check_sheet_cell_id(id);
  return m_sheet_cells.at(id);
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

const char *ElementRegistry::intern_font_name(const std::string &name) {
  if (const auto it = std::ranges::find(m_font_names, name);
      it != m_font_names.end()) {
    return it->c_str();
  }
  return m_font_names.emplace_back(name).c_str();
}

void ElementRegistry::set_cell_styles(std::vector<CellStyle> styles) noexcept {
  m_cell_styles = std::move(styles);
}

const ElementRegistry::CellStyle &
ElementRegistry::cell_style_at(const std::uint16_t ixfe) const {
  if (ixfe >= m_cell_styles.size()) {
    throw std::out_of_range(
        "ElementRegistry::cell_style_at: XF index out of range");
  }
  return m_cell_styles[ixfe];
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
    throw std::out_of_range(
        "ElementRegistry::check_id: text identifier not found");
  }
}

void ElementRegistry::check_sheet_id(const ElementIdentifier id) const {
  check_element_id(id);
  if (!m_sheets.contains(id)) {
    throw std::out_of_range(
        "ElementRegistry::check_id: sheet identifier not found");
  }
}

void ElementRegistry::check_sheet_cell_id(const ElementIdentifier id) const {
  check_element_id(id);
  if (!m_sheet_cells.contains(id)) {
    throw std::out_of_range(
        "ElementRegistry::check_id: sheet cell identifier not found");
  }
}

} // namespace odr::internal::oldms::spreadsheet
