#pragma once

#include <odr/definitions.hpp>
#include <odr/document_element.hpp>
#include <odr/table_dimension.hpp>
#include <odr/table_position.hpp>

#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace odr::internal::oldms::spreadsheet {

class ElementRegistry final {
public:
  struct Element final {
    ElementIdentifier parent_id{null_element_id};
    ElementIdentifier first_child_id{null_element_id};
    ElementIdentifier last_child_id{null_element_id};
    ElementIdentifier previous_sibling_id{null_element_id};
    ElementIdentifier next_sibling_id{null_element_id};
    ElementType type{ElementType::none};
  };

  struct Text final {
    std::string text;
  };

  struct Sheet final {
    std::string name;
    /// Used range from the Dimensions record.
    TableDimensions dimensions;
    /// Tight extent of the non-empty cells (rows/columns past the last cell
    /// with content); can be smaller than `dimensions`.
    TableDimensions content;

    std::unordered_map<TablePosition, ElementIdentifier> cells;

    [[nodiscard]] ElementIdentifier cell(std::uint32_t column,
                                         std::uint32_t row) const;
  };

  struct SheetCell final {
    TablePosition position;
  };

  void clear() noexcept;

  [[nodiscard]] std::size_t size() const noexcept;

  std::tuple<ElementIdentifier, Element &> create_element(ElementType type);
  std::tuple<ElementIdentifier, Element &, Text &> create_text_element();
  std::tuple<ElementIdentifier, Element &, Sheet &> create_sheet_element();
  std::tuple<ElementIdentifier, Element &, SheetCell &>
  create_sheet_cell_element(const TablePosition &position);

  [[nodiscard]] Element &element_at(ElementIdentifier id);
  [[nodiscard]] Text &text_element_at(ElementIdentifier id);
  [[nodiscard]] Sheet &sheet_element_at(ElementIdentifier id);
  [[nodiscard]] SheetCell &sheet_cell_element_at(ElementIdentifier id);

  [[nodiscard]] const Element &element_at(ElementIdentifier id) const;
  [[nodiscard]] const Text &text_element_at(ElementIdentifier id) const;
  [[nodiscard]] const Sheet &sheet_element_at(ElementIdentifier id) const;
  [[nodiscard]] const SheetCell &
  sheet_cell_element_at(ElementIdentifier id) const;

  void append_child(ElementIdentifier parent_id, ElementIdentifier child_id);
  /// Registers a cell with its sheet: sets the cell's parent and adds it to
  /// the sheet's position lookup. Cells are not part of the sibling chain;
  /// they are addressed via `Sheet::cell` (like the ooxml/spreadsheet module).
  void append_sheet_cell(ElementIdentifier sheet_id, ElementIdentifier cell_id);

private:
  std::vector<Element> m_elements;
  std::unordered_map<ElementIdentifier, Text> m_texts;
  std::unordered_map<ElementIdentifier, Sheet> m_sheets;
  std::unordered_map<ElementIdentifier, SheetCell> m_sheet_cells;

  void check_element_id(ElementIdentifier id) const;
  void check_text_id(ElementIdentifier id) const;
  void check_sheet_id(ElementIdentifier id) const;
  void check_sheet_cell_id(ElementIdentifier id) const;
};

} // namespace odr::internal::oldms::spreadsheet
