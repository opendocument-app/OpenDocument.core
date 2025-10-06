#pragma once

#include <odr/document_element.hpp>
#include <odr/document_element_identifier.hpp>
#include <odr/style.hpp>

#include <map>
#include <unordered_map>
#include <vector>

#include <pugixml.hpp>

namespace odr::internal::odf {

class ElementRegistry final {
public:
  struct Element final {
    ExtendedElementIdentifier parent_id;
    ElementIdentifier first_child_id{null_element_id};
    ElementIdentifier last_child_id{null_element_id};
    ElementIdentifier previous_sibling_id{null_element_id};
    ElementIdentifier next_sibling_id{null_element_id};
    ElementType type{ElementType::none};
    pugi::xml_node node;
    bool is_editable{false};
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
      bool is_repeated{false};
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

    void create_column(std::uint32_t column, std::uint32_t repeated,
                       pugi::xml_node element);
    void create_row(std::uint32_t row, std::uint32_t repeated,
                    pugi::xml_node element);
    void create_cell(std::uint32_t column, std::uint32_t row,
                     std::uint32_t columns_repeated,
                     std::uint32_t rows_repeated, pugi::xml_node element);

    [[nodiscard]] pugi::xml_node column(std::uint32_t) const;
    [[nodiscard]] pugi::xml_node row(std::uint32_t) const;
    [[nodiscard]] pugi::xml_node cell(std::uint32_t column,
                                      std::uint32_t row) const;
  };

  void clear() noexcept;

  [[nodiscard]] std::size_t size() const noexcept;

  ExtendedElementIdentifier create_element();
  Table &create_table_element(ExtendedElementIdentifier id);
  Text &create_text_element(ExtendedElementIdentifier id);
  Sheet &create_sheet_element(ExtendedElementIdentifier id);

  [[nodiscard]] Element &element(ExtendedElementIdentifier id);
  [[nodiscard]] const Element &element(ExtendedElementIdentifier id) const;

  [[nodiscard]] Table &table_element(ExtendedElementIdentifier id);
  [[nodiscard]] const Table &table_element(ExtendedElementIdentifier id) const;

  [[nodiscard]] Text &text_element(ExtendedElementIdentifier id);
  [[nodiscard]] const Text &text_element(ExtendedElementIdentifier id) const;

  [[nodiscard]] Sheet &sheet_element(ExtendedElementIdentifier id);
  [[nodiscard]] const Sheet &sheet_element(ExtendedElementIdentifier id) const;

  void append_child(ExtendedElementIdentifier parent_id,
                    ExtendedElementIdentifier child_id);
  void append_column(ExtendedElementIdentifier table_id,
                     ExtendedElementIdentifier column_id);
  void append_shape(ExtendedElementIdentifier sheet_id,
                    ExtendedElementIdentifier shape_id);

private:
  std::vector<Element> m_elements;
  std::unordered_map<ElementIdentifier, Table> m_tables;
  std::unordered_map<ElementIdentifier, Text> m_texts;
  std::unordered_map<ElementIdentifier, Sheet> m_sheets;

  void check_element_id(ExtendedElementIdentifier id) const;
  void check_table_id(ExtendedElementIdentifier id) const;
  void check_text_id(ExtendedElementIdentifier id) const;
  void check_sheet_id(ExtendedElementIdentifier id) const;
};

} // namespace odr::internal::odf
