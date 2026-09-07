#pragma once

#include <odr/definitions.hpp>
#include <odr/document_element.hpp>

#include <odr/internal/common/element_registry.hpp>
#include <odr/internal/common/list_numbering.hpp>
#include <odr/table_dimension.hpp>
#include <odr/table_position.hpp>

#include <cstdint>
#include <span>
#include <tuple>
#include <vector>

#include <pugixml.hpp>

namespace odr::internal::odf {

/// An element is five ids, so the width is most of what one costs. Widened
/// back at every boundary; an id past `check_element_id` fits.
using StoredId = std::uint32_t;

struct RegistryElement final : ElementNode<StoredId> {
  pugi::xml_node node;
};

/// One element stands for every position a repeat covers ([ODF 1.2] 19.297,
/// 19.302), so the position rides in the id rather than an index. Resolved
/// through the sheet's cell index, so a handle follows it.
namespace positional_id {

/// `tag | ordinal(15) | column(24) | row(24)`, the ordinal in document order.
constexpr ElementIdentifier tag = ElementIdentifier{1} << 63;
constexpr std::uint64_t column_shift = 24;
constexpr std::uint64_t ordinal_shift = 48;
constexpr std::uint64_t row_max = (std::uint64_t{1} << column_shift) - 1;
constexpr std::uint64_t column_max =
    (std::uint64_t{1} << (ordinal_shift - column_shift)) - 1;
constexpr std::uint64_t ordinal_max = (std::uint64_t{1} << 15) - 1;

/// Null where a field is too wide; the caller then keeps the index.
constexpr ElementIdentifier make(const std::uint32_t ordinal,
                                 const std::uint32_t column,
                                 const std::uint32_t row) noexcept {
  if (ordinal > ordinal_max || column > column_max || row > row_max) {
    return null_element_id;
  }
  return tag | static_cast<ElementIdentifier>(ordinal) << ordinal_shift |
         static_cast<ElementIdentifier>(column) << column_shift |
         static_cast<ElementIdentifier>(row);
}

constexpr bool holds(const ElementIdentifier id) noexcept {
  return (id & tag) != 0;
}

constexpr std::uint32_t ordinal_of(const ElementIdentifier id) noexcept {
  return static_cast<std::uint32_t>(id >> ordinal_shift & ordinal_max);
}

constexpr std::uint32_t column_of(const ElementIdentifier id) noexcept {
  return static_cast<std::uint32_t>(id >> column_shift & column_max);
}

constexpr std::uint32_t row_of(const ElementIdentifier id) noexcept {
  return static_cast<std::uint32_t>(id & row_max);
}

} // namespace positional_id

class ElementRegistry final
    : public internal::ElementRegistry<RegistryElement, StoredId> {
public:
  struct Table final {
    StoredId first_column_id{null_element_id};
    StoredId last_column_id{null_element_id};
  };

  struct Text final {
    pugi::xml_node last;
  };

  /// Columns, rows and cells keyed by the *end* of the range they repeat over
  /// and resolved with an upper bound, so a run of 5000 is one entry. Sorted
  /// vectors, not maps: parsing appends in document order. The cells of every
  /// row live in one array per sheet, each row holding where its run starts.
  struct Sheet final {
    struct Column final {
      std::uint32_t end{0};
      pugi::xml_node node;
    };

    struct Cell final {
      std::uint32_t end{0};
      StoredId element_id{null_element_id};
      pugi::xml_node node;
    };

    struct Row final {
      std::uint32_t end{0};
      std::uint32_t first_cell{0};
      pugi::xml_node node;
    };

    TableDimensions dimensions;

    /// Its place in document order, which is what a cell id names it by.
    std::uint32_t ordinal{0};

    std::vector<Column> columns;
    std::vector<Row> rows;
    std::vector<Cell> cells;

    StoredId first_shape_id{null_element_id};
    StoredId last_shape_id{null_element_id};

    void register_column(std::uint32_t column, std::uint32_t repeated,
                         pugi::xml_node element);
    void register_row(std::uint32_t row, std::uint32_t repeated,
                      pugi::xml_node element);
    /// Has to follow the @ref register_row of the row it belongs to.
    void register_cell(std::uint32_t column, std::uint32_t row,
                       std::uint32_t columns_repeated,
                       std::uint32_t rows_repeated, pugi::xml_node element,
                       ElementIdentifier element_id);

    [[nodiscard]] const Column *column(std::uint32_t column) const;
    [[nodiscard]] const Row *row(std::uint32_t row) const;
    [[nodiscard]] const Cell *cell(std::uint32_t column,
                                   std::uint32_t row) const;

    /// The cells of @p row - one of this sheet's `rows` - in column order.
    [[nodiscard]] std::span<const Cell> row_cells(const Row &row) const;

    [[nodiscard]] pugi::xml_node column_node(std::uint32_t column) const;
    [[nodiscard]] pugi::xml_node row_node(std::uint32_t row) const;
    [[nodiscard]] pugi::xml_node cell_node(std::uint32_t column,
                                           std::uint32_t row) const;
  };

  struct SheetCell final {
    TablePosition position;
    bool is_repeated{false};
  };

  std::tuple<ElementIdentifier, Element &> create_element(ElementType type,
                                                          pugi::xml_node node);
  /// A `frame` drawing @p shape_type rather than being a plain box.
  std::tuple<ElementIdentifier, Element &>
  create_shape_element(ShapeType shape_type, pugi::xml_node node);
  std::tuple<ElementIdentifier, Element &, Text &>
  create_text_element(pugi::xml_node first_node, pugi::xml_node last_node);
  std::tuple<ElementIdentifier, Element &, Table &>
  create_table_element(pugi::xml_node node);
  std::tuple<ElementIdentifier, Element &, Sheet &>
  create_sheet_element(pugi::xml_node node);
  std::tuple<ElementIdentifier, Element &, SheetCell &>
  create_sheet_cell_element(pugi::xml_node node, const TablePosition &position,
                            bool is_repeated);

  /// Null where the position no longer holds a cell.
  [[nodiscard]] ElementIdentifier resolve_id(const ElementIdentifier id) const {
    if (!positional_id::holds(id)) {
      return id;
    }
    const Sheet &sheet =
        m_sheets.at(m_sheet_ids.at(positional_id::ordinal_of(id)));
    const Sheet::Cell *cell =
        sheet.cell(positional_id::column_of(id), positional_id::row_of(id));
    return cell != nullptr ? cell->element_id : null_element_id;
  }

  [[nodiscard]] auto &text_element_at(this auto &self,
                                      const ElementIdentifier id) {
    return self.m_texts.at(self.resolve_id(id));
  }
  [[nodiscard]] auto &table_element_at(this auto &self,
                                       const ElementIdentifier id) {
    return self.m_tables.at(self.resolve_id(id));
  }
  [[nodiscard]] auto &sheet_element_at(this auto &self,
                                       const ElementIdentifier id) {
    return self.m_sheets.at(self.resolve_id(id));
  }

  [[nodiscard]] const SheetCell &
  sheet_cell_element_at(const ElementIdentifier id) const {
    return m_sheet_cells.at(resolve_id(id));
  }

  [[nodiscard]] const SheetCell *
  sheet_cell_element(const ElementIdentifier id) const {
    return m_sheet_cells.find(resolve_id(id));
  }

  /// The id a handle for (@p column, @p row) carries - the index itself unless
  /// the cell there is repeated.
  [[nodiscard]] ElementIdentifier sheet_cell_id(ElementIdentifier sheet_id,
                                                std::uint32_t column,
                                                std::uint32_t row) const;

  [[nodiscard]] ShapeType shape_type(ElementIdentifier id) const;

  void set_list_type(ElementIdentifier id, ListType type);
  void set_list_marker(ElementIdentifier id, ListMarker marker);

  [[nodiscard]] ListType list_type(ElementIdentifier id) const;
  [[nodiscard]] const ListMarker &list_marker(ElementIdentifier id) const;

  void append_column(ElementIdentifier table_id, ElementIdentifier column_id);
  void append_shape(ElementIdentifier sheet_id, ElementIdentifier shape_id);
  void append_sheet_cell(ElementIdentifier sheet_id, ElementIdentifier cell_id);

private:
  /// The sheets in document order, so a cell id can name one in 15 bits.
  std::vector<StoredId> m_sheet_ids;

  SortedSideTable<Text, StoredId> m_texts;
  SortedSideTable<Table, StoredId> m_tables;
  SortedSideTable<Sheet, StoredId> m_sheets;
  SortedSideTable<SheetCell, StoredId> m_sheet_cells;
  SortedSideTable<ShapeType, StoredId> m_shape_types;
  // out of id order: written when a list is resolved, not when it is parsed
  SideTable<ListType> m_list_types;
  SideTable<ListMarker> m_list_markers;
};

} // namespace odr::internal::odf
