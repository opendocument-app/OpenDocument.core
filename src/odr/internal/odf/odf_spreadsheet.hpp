#ifndef ODR_INTERNAL_ODF_SPREADSHEET_H
#define ODR_INTERNAL_ODF_SPREADSHEET_H

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
class SheetColumn;
class SheetRow;
class SheetCell;

class SpreadsheetRoot final : public Root {
public:
  using Root::Root;
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

  abstract::SheetColumn *column(const abstract::Document *,
                                std::uint32_t column) const final;
  abstract::SheetRow *row(const abstract::Document *,
                          std::uint32_t row) const final;
  abstract::SheetCell *cell(const abstract::Document *, std::uint32_t column,
                            std::uint32_t row) const final;

  [[nodiscard]] abstract::Element *
  first_shape(const abstract::Document *) const final;

  [[nodiscard]] TableStyle style(const abstract::Document *) const final;

  void init_column_(std::uint32_t column, std::uint32_t repeated,
                    SheetColumn *element);
  void init_row_(std::uint32_t row, std::uint32_t repeated, SheetRow *element);
  void init_cell_(std::uint32_t column, std::uint32_t row,
                  std::uint32_t columns_repeated, std::uint32_t rows_repeated,
                  SheetCell *element);
  void init_dimensions_(TableDimensions dimensions);

private:
  struct Row {
    abstract::SheetRow *element{nullptr};
    std::map<std::uint32_t, abstract::SheetCell *> cells;
  };

  TableDimensions m_dimensions;

  std::map<std::uint32_t, abstract::SheetColumn *> m_columns;
  std::map<std::uint32_t, Row> m_rows;

  Element *m_first_shape{nullptr};
};

template <>
std::tuple<Sheet *, pugi::xml_node>
parse_element_tree<Sheet>(Document &document, pugi::xml_node node);

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_SPREADSHEET_H
