#pragma once

#include <odr/internal/common/element_registry.hpp>
#include <odr/internal/common/path.hpp>

#include <odr/internal/ooxml/ooxml_util.hpp>

#include <odr/definitions.hpp>
#include <odr/document_element.hpp>
#include <odr/table_dimension.hpp>
#include <odr/table_position.hpp>

#include <map>
#include <string>
#include <tuple>
#include <unordered_map>

#include <pugixml.hpp>

namespace odr::internal::ooxml::spreadsheet {

struct RegistryElement final : ElementNode<ElementIdentifier> {
  pugi::xml_node node;
};

class ElementRegistry final
    : public internal::ElementRegistry<RegistryElement> {
public:
  struct ElementRelations final {
    const Relations *relations{nullptr};
    AbsPath origin;
  };

  struct Text final {
    pugi::xml_node last;
  };

  struct Sheet final {
    struct Column final {
      pugi::xml_node node;
    };

    struct Row final {
      pugi::xml_node node;
    };

    struct Cell final {
      pugi::xml_node node;
      ElementIdentifier element_id{null_element_id};
    };

    /// From the workbook's `<sheet name=…>`; the worksheet part carries none.
    std::string name;

    TableDimensions dimensions;

    std::map<std::uint32_t, Column> columns;
    std::unordered_map<std::uint32_t, Row> rows;
    std::unordered_map<TablePosition, Cell> cells;

    ElementIdentifier first_shape_id{null_element_id};
    ElementIdentifier last_shape_id{null_element_id};

    void register_column(std::uint32_t column_min, std::uint32_t column_max,
                         pugi::xml_node element);
    void register_row(std::uint32_t row, pugi::xml_node element);
    void register_cell(std::uint32_t column, std::uint32_t row,
                       pugi::xml_node element, ElementIdentifier element_id);

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
    TableDimensions span{1, 1};
    bool is_covered{false};
  };

  std::tuple<ElementIdentifier, Element &> create_element(ElementType type,
                                                          pugi::xml_node node);
  std::tuple<ElementIdentifier, Element &, Text &>
  create_text_element(pugi::xml_node first_node, pugi::xml_node last_node);
  std::tuple<ElementIdentifier, Element &, Sheet &>
  create_sheet_element(pugi::xml_node node);
  std::tuple<ElementIdentifier, Element &, SheetCell &>
  create_sheet_cell_element(pugi::xml_node node, const TablePosition &position);

  ElementRelations &attach_element_relations(ElementIdentifier id,
                                             const Relations &relations,
                                             const AbsPath &origin);

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

  [[nodiscard]] const ElementRelations *
  element_relations(const ElementIdentifier id) const {
    return m_element_relations.find(id);
  }

  void append_shape(ElementIdentifier sheet_id, ElementIdentifier shape_id);
  void append_sheet_cell(ElementIdentifier sheet_id, ElementIdentifier cell_id);

private:
  SideTable<ElementRelations> m_element_relations;
  SideTable<Text> m_texts;
  SideTable<Sheet> m_sheets;
  SideTable<SheetCell> m_sheet_cells;
};

} // namespace odr::internal::ooxml::spreadsheet
