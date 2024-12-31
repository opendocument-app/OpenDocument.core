#pragma once

#include <odr/internal/abstract/document.hpp>
#include <odr/internal/abstract/sheet_element.hpp>
#include <odr/internal/common/style.hpp>
#include <odr/internal/common/table_position.hpp>
#include <odr/internal/odf/odf_element.hpp>
#include <odr/internal/odf/odf_parser.hpp>

#include <map>
#include <memory>
#include <unordered_map>

namespace pugi {
class xml_node;
}

namespace odr::internal::odf {
class Document;
class SheetCell;

class SpreadsheetRoot final : public Root {
public:
  using Root::Root;
};

struct SheetIndex final {
  struct Row {
    pugi::xml_node row;
    std::map<std::uint32_t, pugi::xml_node> cells;
  };

  TableDimensions dimensions;

  std::map<std::uint32_t, pugi::xml_node> columns;
  std::map<std::uint32_t, Row> rows;

  void init_column(std::uint32_t column, std::uint32_t repeated,
                   pugi::xml_node element);
  void init_row(std::uint32_t row, std::uint32_t repeated,
                pugi::xml_node element);
  void init_cell(std::uint32_t column, std::uint32_t row,
                 std::uint32_t columns_repeated, std::uint32_t rows_repeated,
                 pugi::xml_node element);

  pugi::xml_node column(std::uint32_t) const;
  pugi::xml_node row(std::uint32_t) const;
  pugi::xml_node cell(std::uint32_t column, std::uint32_t row) const;
};

class Sheet final : public Element, public abstract::Sheet {
public:
  using Element::Element;

  [[nodiscard]] std::string name(const abstract::Document *) const final;

  [[nodiscard]] TableDimensions
  dimensions(const abstract::Document *) const final;

  [[nodiscard]] TableDimensions
  content(const abstract::Document *,
          const std::optional<TableDimensions> range) const final;

  abstract::SheetCell *cell(const abstract::Document *, std::uint32_t column,
                            std::uint32_t row) const final;

  [[nodiscard]] abstract::Element *
  first_shape(const abstract::Document *) const final;

  [[nodiscard]] TableStyle style(const abstract::Document *) const final;
  [[nodiscard]] TableColumnStyle column_style(const abstract::Document *,
                                              std::uint32_t column) const final;
  [[nodiscard]] TableRowStyle row_style(const abstract::Document *,
                                        std::uint32_t row) const final;
  [[nodiscard]] TableCellStyle cell_style(const abstract::Document *,
                                          std::uint32_t column,
                                          std::uint32_t row) const final;

  void init_column_(std::uint32_t column, std::uint32_t repeated,
                    pugi::xml_node element);
  void init_row_(std::uint32_t row, std::uint32_t repeated,
                 pugi::xml_node element);
  void init_cell_(std::uint32_t column, std::uint32_t row,
                  std::uint32_t columns_repeated, std::uint32_t rows_repeated,
                  pugi::xml_node element);
  void init_cell_element_(std::uint32_t column, std::uint32_t row,
                          SheetCell *element);
  void init_dimensions_(TableDimensions dimensions);
  void append_shape_(Element *shape);

  common::ResolvedStyle cell_style_(const abstract::Document *,
                                    std::uint32_t column,
                                    std::uint32_t row) const;

private:
  SheetIndex m_index;

  std::unordered_map<common::TablePosition, SheetCell *> m_cells;
  Element *m_first_shape{nullptr};
  Element *m_last_shape{nullptr};
};

template <>
std::tuple<Sheet *, pugi::xml_node>
parse_element_tree<Sheet>(Document &document, pugi::xml_node node);

} // namespace odr::internal::odf
