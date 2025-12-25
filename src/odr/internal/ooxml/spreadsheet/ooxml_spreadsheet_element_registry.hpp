#pragma once

#include <odr/definitions.hpp>
#include <odr/document_element.hpp>
#include <odr/table_dimension.hpp>

#include <map>
#include <unordered_map>
#include <vector>

#include <pugixml.hpp>

namespace odr::internal::ooxml::spreadsheet {

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
    bool is_editable{false};
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

    void create_column(std::uint32_t column_min, std::uint32_t column_max,
                       pugi::xml_node element);
    void create_row(std::uint32_t row, pugi::xml_node element);
    void create_cell(std::uint32_t column, std::uint32_t row,
                     pugi::xml_node element);

    [[nodiscard]] pugi::xml_node column(std::uint32_t) const;
    [[nodiscard]] pugi::xml_node row(std::uint32_t) const;
    [[nodiscard]] pugi::xml_node cell(std::uint32_t column,
                                      std::uint32_t row) const;
  };

  void clear() noexcept;

  [[nodiscard]] std::size_t size() const noexcept;

  ElementIdentifier create_element();
  Text &create_text_element(ElementIdentifier id);
  Sheet &create_sheet_element(ElementIdentifier id);

  [[nodiscard]] Element &element(ElementIdentifier id);
  [[nodiscard]] const Element &element(ElementIdentifier id) const;

  [[nodiscard]] Text &text_element(ElementIdentifier id);
  [[nodiscard]] const Text &text_element(ElementIdentifier id) const;

  [[nodiscard]] Sheet &sheet_element(ElementIdentifier id);
  [[nodiscard]] const Sheet &sheet_element(ElementIdentifier id) const;

  void append_child(ElementIdentifier parent_id, ElementIdentifier child_id);
  void append_shape(ElementIdentifier sheet_id, ElementIdentifier shape_id);
  void append_sheet_cell(ElementIdentifier sheet_id, ElementIdentifier cell_id);

private:
  std::vector<Element> m_elements;
  std::unordered_map<ElementIdentifier, Text> m_texts;
  std::unordered_map<ElementIdentifier, Sheet> m_sheets;

  void check_element_id(ElementIdentifier id) const;
  void check_text_id(ElementIdentifier id) const;
  void check_sheet_id(ElementIdentifier id) const;
};

} // namespace odr::internal::ooxml::spreadsheet
