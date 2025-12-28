#pragma once

#include <odr/definitions.hpp>
#include <odr/document_element.hpp>
#include <odr/table_dimension.hpp>
#include <odr/table_position.hpp>

#include <map>
#include <unordered_map>
#include <vector>

#include <pugixml.hpp>

namespace odr::internal::odf {

class ElementRegistry final {
public:
  struct Element final {
    ElementIdentifier parent_id{null_element_id};
    ElementIdentifier first_child_id{null_element_id};
    ElementIdentifier last_child_id{null_element_id};
    ElementIdentifier previous_sibling_id{null_element_id};
    ElementIdentifier next_sibling_id{null_element_id};
    ElementType type{ElementType::none};
    pugi::xml_node node;
  };

  struct Table final {
    ElementIdentifier first_column_id{null_element_id};
    ElementIdentifier last_column_id{null_element_id};
  };

  struct Text final {
    pugi::xml_node last;
  };

  struct Sheet final {
    struct Column final {
      pugi::xml_node node;
    };

    struct Cell final {
      pugi::xml_node node;
      ElementIdentifier element_id{null_element_id};
    };

    struct Row final {
      pugi::xml_node node;
      std::map<std::uint32_t, Cell> cells;
    };

    TableDimensions dimensions;

    std::map<std::uint32_t, Column> columns;
    std::map<std::uint32_t, Row> rows;

    ElementIdentifier first_shape_id{null_element_id};
    ElementIdentifier last_shape_id{null_element_id};

    void register_column(std::uint32_t column, std::uint32_t repeated,
                         pugi::xml_node element);
    void register_row(std::uint32_t row, std::uint32_t repeated,
                      pugi::xml_node element);
    void register_cell(std::uint32_t column, std::uint32_t row,
                       std::uint32_t columns_repeated,
                       std::uint32_t rows_repeated, pugi::xml_node element,
                       ElementIdentifier element_id);

    [[nodiscard]] const Column *column(std::uint32_t column) const;
    [[nodiscard]] const Row *row(std::uint32_t row) const;
    [[nodiscard]] const Cell *cell(std::uint32_t column,
                                   std::uint32_t row) const;

    [[nodiscard]] pugi::xml_node column_node(std::uint32_t column) const;
    [[nodiscard]] pugi::xml_node row_node(std::uint32_t row) const;
    [[nodiscard]] pugi::xml_node cell_node(std::uint32_t column,
                                           std::uint32_t row) const;
  };

  struct SheetCell final {
    TablePosition position;
    bool is_repeated{false};
  };

  void clear() noexcept;

  [[nodiscard]] std::size_t size() const noexcept;

  std::tuple<ElementIdentifier, Element &> create_element(ElementType type,
                                                          pugi::xml_node node);
  std::tuple<ElementIdentifier, Element &, Text &>
  create_text_element(pugi::xml_node first_node, pugi::xml_node last_node);
  std::tuple<ElementIdentifier, Element &, Table &>
  create_table_element(pugi::xml_node node);
  std::tuple<ElementIdentifier, Element &, Sheet &>
  create_sheet_element(pugi::xml_node node);
  std::tuple<ElementIdentifier, Element &, SheetCell &>
  create_sheet_cell_element(pugi::xml_node node, const TablePosition &position,
                            bool is_repeated);

  [[nodiscard]] Element &element_at(ElementIdentifier id);
  [[nodiscard]] Table &table_element_at(ElementIdentifier id);
  [[nodiscard]] Sheet &sheet_element_at(ElementIdentifier id);

  [[nodiscard]] const Element &element_at(ElementIdentifier id) const;
  [[nodiscard]] const Table &table_element_at(ElementIdentifier id) const;
  [[nodiscard]] const Sheet &sheet_element_at(ElementIdentifier id) const;

  [[nodiscard]] Element *element(ElementIdentifier id);
  [[nodiscard]] Text *text_element(ElementIdentifier id);

  [[nodiscard]] const Element *element(ElementIdentifier id) const;
  [[nodiscard]] const Text *text_element(ElementIdentifier id) const;
  [[nodiscard]] const Table *table_element(ElementIdentifier id) const;
  [[nodiscard]] const Sheet *sheet_element(ElementIdentifier id) const;
  [[nodiscard]] const SheetCell *sheet_cell_element(ElementIdentifier id) const;

  void append_child(ElementIdentifier parent_id, ElementIdentifier child_id);
  void append_column(ElementIdentifier table_id, ElementIdentifier column_id);
  void append_shape(ElementIdentifier sheet_id, ElementIdentifier shape_id);
  void append_sheet_cell(ElementIdentifier sheet_id, ElementIdentifier cell_id);

private:
  std::vector<Element> m_elements;
  std::unordered_map<ElementIdentifier, Text> m_texts;
  std::unordered_map<ElementIdentifier, Table> m_tables;
  std::unordered_map<ElementIdentifier, Sheet> m_sheets;
  std::unordered_map<ElementIdentifier, SheetCell> m_sheet_cells;

  void check_element_id(ElementIdentifier id) const;
  void check_table_id(ElementIdentifier id) const;
  void check_text_id(ElementIdentifier id) const;
  void check_sheet_id(ElementIdentifier id) const;
  void check_sheet_cell_id(ElementIdentifier id) const;
};

} // namespace odr::internal::odf
