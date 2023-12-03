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
  explicit SpreadsheetRoot(pugi::xml_node node);
};

class Sheet final : public Element, public common::Sheet {
public:
  explicit Sheet(pugi::xml_node node);

  [[nodiscard]] std::string name(const abstract::Document *) const final;

  [[nodiscard]] TableDimensions
  dimensions(const abstract::Document *) const final;

  [[nodiscard]] TableDimensions
  content(const abstract::Document *,
          const std::optional<TableDimensions> range) const final;

  [[nodiscard]] abstract::Element *column(const abstract::Document *,
                                          std::uint32_t column) const final;
  [[nodiscard]] abstract::Element *row(const abstract::Document *,
                                       std::uint32_t row) const final;
  [[nodiscard]] abstract::Element *cell(const abstract::Document *,
                                        std::uint32_t column,
                                        std::uint32_t row) const final;

  [[nodiscard]] abstract::Element *
  first_shape(const abstract::Document *) const final;

  [[nodiscard]] TableStyle style(const abstract::Document *) const final;

  void init_column(std::uint32_t column, std::uint32_t repeated,
                   Element *element);
  void init_row(std::uint32_t row, std::uint32_t repeated, Element *element);
  void init_cell(std::uint32_t column, std::uint32_t row,
                 std::uint32_t columns_repeated, std::uint32_t rows_repeated,
                 Element *element);
  void init_dimensions(TableDimensions dimensions);

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
std::tuple<odf::Element *, pugi::xml_node>
parse_element_tree<Sheet>(pugi::xml_node node,
                          std::vector<std::unique_ptr<Element>> &store);

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_SPREADSHEET_H
