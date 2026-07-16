#pragma once

#include <odr/definitions.hpp>
#include <odr/document_element.hpp>
#include <odr/style.hpp>
#include <odr/table_dimension.hpp>
#include <odr/table_position.hpp>

#include <deque>
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
    /// Index into the workbook's cell styles (the cell record's ixfe).
    std::uint16_t ixfe{0};
  };

  /// Display properties resolved from an XF record and its Font: the text
  /// (font) side and the cell (fill) side.
  struct CellStyle final {
    TextStyle text_style;
    TableCellStyle cell_style;
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

  /// Stores a font name and returns a pointer that stays valid for the
  /// registry's lifetime (`TextStyle::font_name` is a `const char *`).
  const char *intern_font_name(const std::string &name);

  void set_cell_styles(std::vector<CellStyle> styles) noexcept;
  /// The resolved style of an XF record, by XF index (a cell's ixfe).
  /// Throws if the index has no XF record.
  [[nodiscard]] const CellStyle &cell_style_at(std::uint16_t ixfe) const;

private:
  std::vector<Element> m_elements;
  std::unordered_map<ElementIdentifier, Text> m_texts;
  std::unordered_map<ElementIdentifier, Sheet> m_sheets;
  std::unordered_map<ElementIdentifier, SheetCell> m_sheet_cells;
  std::vector<CellStyle> m_cell_styles;
  /// Deque: elements never move, so interned `c_str()`s stay valid.
  std::deque<std::string> m_font_names;

  void check_element_id(ElementIdentifier id) const;
  void check_text_id(ElementIdentifier id) const;
  void check_sheet_id(ElementIdentifier id) const;
  void check_sheet_cell_id(ElementIdentifier id) const;
};

} // namespace odr::internal::oldms::spreadsheet
