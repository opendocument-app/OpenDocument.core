#pragma once

#include <odr/definitions.hpp>
#include <odr/document_element.hpp>

#include <odr/internal/common/element_registry.hpp>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <utility>

namespace odr::internal::iwork {

class ElementRegistry final
    : public internal::ElementRegistry<ElementNode<ElementIdentifier>> {
public:
  struct Size final {
    float width{};
    float height{};
  };

  /// A drawable's rectangle, in points. A side of the extent is absent for a
  /// box that grows with its text rather than one of zero extent.
  struct Rect final {
    float x{};
    float y{};
    std::optional<float> width{};
    std::optional<float> height{};
  };

  struct Text final {
    std::string text;
  };

  /// Where a drawable sits on its page, in points. Absent for one whose
  /// geometry we did not find.
  struct Frame final {
    std::optional<Rect> rect;
  };

  /// A slide's name and the size of the page it is shown on, in points.
  struct Slide final {
    std::string name;
    std::optional<Size> size;
  };

  /// Where a cell sits in the table or sheet it belongs to, and what kind of
  /// value it holds.
  struct Cell final {
    std::uint32_t row{};
    std::uint32_t column{};
    ValueType value_type{ValueType::unknown};
  };

  /// A table anchored in a text flow. Its rows are its children; its columns
  /// are a chain of their own, as they are in `odf`.
  struct Table final {
    std::uint32_t rows{};
    std::uint32_t columns{};
    ElementIdentifier first_column_id{null_element_id};
    ElementIdentifier last_column_id{null_element_id};
  };

  /// One odr sheet, which is one Numbers table. Cells are reached by
  /// coordinate rather than by walking, so they are not in the child chain —
  /// that is what a sheet's shapes would use.
  struct Sheet final {
    std::string name;
    std::uint32_t rows{};
    std::uint32_t columns{};
    /// The extent that actually holds something, which is what a sheet of a
    /// hundred empty rows should render.
    std::uint32_t content_rows{};
    std::uint32_t content_columns{};
    std::map<std::pair<std::uint32_t, std::uint32_t>, ElementIdentifier> cells;

    [[nodiscard]] ElementIdentifier cell(std::uint32_t column,
                                         std::uint32_t row) const;
  };

  std::tuple<ElementIdentifier, Element &> create_element(ElementType type);
  std::tuple<ElementIdentifier, Element &, Text &> create_text_element();
  std::tuple<ElementIdentifier, Element &, Frame &> create_frame_element();
  std::tuple<ElementIdentifier, Element &, Slide &> create_slide_element();
  std::tuple<ElementIdentifier, Element &, Table &> create_table_element();
  std::tuple<ElementIdentifier, Element &, Sheet &> create_sheet_element();
  std::tuple<ElementIdentifier, Element &, Cell &>
  create_cell_element(ElementType type);

  [[nodiscard]] Text &text_element_at(const ElementIdentifier id) {
    return m_texts.at(id);
  }
  [[nodiscard]] Frame &frame_element_at(const ElementIdentifier id) {
    return m_frames.at(id);
  }
  [[nodiscard]] Slide &slide_element_at(const ElementIdentifier id) {
    return m_slides.at(id);
  }
  [[nodiscard]] Table &table_element_at(const ElementIdentifier id) {
    return m_tables.at(id);
  }
  [[nodiscard]] Sheet &sheet_element_at(const ElementIdentifier id) {
    return m_sheets.at(id);
  }
  [[nodiscard]] Cell &cell_element_at(const ElementIdentifier id) {
    return m_cells.at(id);
  }

  [[nodiscard]] const Text &text_element_at(const ElementIdentifier id) const {
    return m_texts.at(id);
  }
  [[nodiscard]] const Frame &
  frame_element_at(const ElementIdentifier id) const {
    return m_frames.at(id);
  }
  [[nodiscard]] const Slide &
  slide_element_at(const ElementIdentifier id) const {
    return m_slides.at(id);
  }
  [[nodiscard]] const Table &
  table_element_at(const ElementIdentifier id) const {
    return m_tables.at(id);
  }
  [[nodiscard]] const Sheet &
  sheet_element_at(const ElementIdentifier id) const {
    return m_sheets.at(id);
  }
  [[nodiscard]] const Cell &cell_element_at(const ElementIdentifier id) const {
    return m_cells.at(id);
  }

  /// Links @p column_id into @p table_id's column chain. Columns are not
  /// children: a table's child chain is its rows.
  void append_table_column(ElementIdentifier table_id,
                           ElementIdentifier column_id);
  /// Files @p cell_id at its coordinate in @p sheet_id. A sheet's cells are
  /// looked up rather than walked, so this only sets the parent.
  void append_sheet_cell(ElementIdentifier sheet_id, ElementIdentifier cell_id);

private:
  SideTable<Text> m_texts;
  SideTable<Frame> m_frames;
  SideTable<Slide> m_slides;
  SideTable<Table> m_tables;
  SideTable<Sheet> m_sheets;
  SideTable<Cell> m_cells;
};

} // namespace odr::internal::iwork
