#ifndef ODR_INTERNAL_ODF_SPREADSHEET_H
#define ODR_INTERNAL_ODF_SPREADSHEET_H

#include <odr/internal/abstract/document.hpp>
#include <odr/internal/common/document_element.hpp>
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

class SpreadsheetRoot final : public Root {
public:
  using Root::Root;
};

class Sheet final : public Element, public common::Sheet {
public:
  using Element::Element;

  [[nodiscard]] std::string name(const abstract::Document *) const final;

  [[nodiscard]] TableDimensions
  dimensions(const abstract::Document *) const final;

  [[nodiscard]] TableDimensions
  content(const abstract::Document *,
          const std::optional<TableDimensions> range) const final;

  [[nodiscard]] abstract::Element *
  first_cell_element(const abstract::Document *, std::uint32_t column,
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

  [[nodiscard]] bool is_covered(const abstract::Document *,
                                std::uint32_t column,
                                std::uint32_t row) const final;
  [[nodiscard]] TableDimensions span(const abstract::Document *,
                                     std::uint32_t column,
                                     std::uint32_t row) const final;
  [[nodiscard]] ValueType value_type(const abstract::Document *,
                                     std::uint32_t column,
                                     std::uint32_t row) const final;

  void init_column_(std::uint32_t column, std::uint32_t repeated,
                    Element *element);
  void init_row_(std::uint32_t row, std::uint32_t repeated, Element *element);
  void init_cell_(std::uint32_t column, std::uint32_t row,
                  std::uint32_t columns_repeated, std::uint32_t rows_repeated,
                  Element *element);
  void init_dimensions_(TableDimensions dimensions);

  [[nodiscard]] abstract::Element *column_(const abstract::Document *,
                                           std::uint32_t column) const;
  [[nodiscard]] abstract::Element *row_(const abstract::Document *,
                                        std::uint32_t row) const;
  [[nodiscard]] abstract::Element *cell_(const abstract::Document *,
                                         std::uint32_t column,
                                         std::uint32_t row) const;

private:
  struct Row {
    Element *element{nullptr};
    std::map<std::uint32_t, Element *> cells;
  };

  TableDimensions m_dimensions;

  std::map<std::uint32_t, Element *> m_columns;
  std::map<std::uint32_t, Row> m_rows;

  Element *m_first_shape{nullptr};
};

template <>
std::tuple<Element *, pugi::xml_node>
parse_element_tree<Sheet>(Document &document, pugi::xml_node node);

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_SPREADSHEET_H
