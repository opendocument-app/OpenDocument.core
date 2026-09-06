#pragma once

#include <odr/definitions.hpp>
#include <odr/document_element.hpp>
#include <odr/table_dimension.hpp>
#include <odr/table_position.hpp>

#include <odr/internal/common/element_registry.hpp>

#include <string>
#include <tuple>
#include <unordered_map>

namespace odr::internal::oldms::spreadsheet {

class ElementRegistry final
    : public internal::ElementRegistry<ElementNode<ElementIdentifier>> {
public:
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
    /// Index into the workbook's XF records (the cell record's ixfe); resolved
    /// through the `StyleRegistry`.
    std::uint16_t ixfe{0};
  };

  std::tuple<ElementIdentifier, Element &> create_element(ElementType type);
  std::tuple<ElementIdentifier, Element &, Text &> create_text_element();
  std::tuple<ElementIdentifier, Element &, Sheet &> create_sheet_element();
  std::tuple<ElementIdentifier, Element &, SheetCell &>
  create_sheet_cell_element(const TablePosition &position);

  [[nodiscard]] Text &text_element_at(const ElementIdentifier id) {
    return m_texts.at(id);
  }
  [[nodiscard]] Sheet &sheet_element_at(const ElementIdentifier id) {
    return m_sheets.at(id);
  }
  [[nodiscard]] SheetCell &sheet_cell_element_at(const ElementIdentifier id) {
    return m_sheet_cells.at(id);
  }

  [[nodiscard]] const Text &text_element_at(const ElementIdentifier id) const {
    return m_texts.at(id);
  }
  [[nodiscard]] const Sheet &
  sheet_element_at(const ElementIdentifier id) const {
    return m_sheets.at(id);
  }
  [[nodiscard]] const SheetCell &
  sheet_cell_element_at(const ElementIdentifier id) const {
    return m_sheet_cells.at(id);
  }

  /// Registers a cell with its sheet: sets the cell's parent and adds it to
  /// the sheet's position lookup. Cells are not part of the sibling chain;
  /// they are addressed via `Sheet::cell` (like the ooxml/spreadsheet module).
  void append_sheet_cell(ElementIdentifier sheet_id, ElementIdentifier cell_id);

private:
  SideTable<Text> m_texts;
  SideTable<Sheet> m_sheets;
  SideTable<SheetCell> m_sheet_cells;
};

} // namespace odr::internal::oldms::spreadsheet
