#include <odr/internal/common/table_cursor.hpp>
#include <odr/internal/odf/odf_spreadsheet.hpp>
#include <stdexcept>

namespace odr::internal::odf {

namespace {

class SheetColumn final : public Element, public abstract::TableColumn {
public:
  explicit SheetColumn(pugi::xml_node node)
      : common::Element(node), Element(node) {}

  [[nodiscard]] TableColumnStyle
  style(const abstract::Document * /*document*/) const final {
    return {}; // TODO
  }
};

class SheetRow final : public Element, public abstract::TableRow {
public:
  explicit SheetRow(pugi::xml_node node)
      : common::Element(node), Element(node) {}

  [[nodiscard]] TableRowStyle
  style(const abstract::Document * /*document*/) const final {
    return {}; // TODO
  }
};

class SheetCell final : public Element, public abstract::TableCell {
public:
  explicit SheetCell(pugi::xml_node node)
      : common::Element(node), Element(node) {}

  [[nodiscard]] abstract::Element *
  column(const abstract::Document * /*document*/) const final {
    return {}; // TODO
  }
  [[nodiscard]] abstract::Element *
  row(const abstract::Document * /*document*/) const final {
    return {}; // TODO
  }

  [[nodiscard]] bool
  covered(const abstract::Document * /*document*/) const final {
    return false; // TODO
  }

  [[nodiscard]] TableDimensions
  span(const abstract::Document * /*document*/) const final {
    return {1, 1}; // TODO
  }

  [[nodiscard]] ValueType
  value_type(const abstract::Document * /*document*/) const final {
    return ValueType::string; // TODO
  }

  [[nodiscard]] TableCellStyle
  style(const abstract::Document * /*document*/) const final {
    return {}; // TODO
  }
};

} // namespace

SpreadsheetRoot::SpreadsheetRoot(pugi::xml_node node)
    : common::Element(node), Root(node) {}

Sheet::Sheet(pugi::xml_node node) : common::Element(node), Element(node) {
  common::TableCursor cursor;

  for (auto column_node : m_node.children("table:table-column")) {
    const auto columns_repeated =
        column_node.attribute("table:number-columns-repeated").as_uint(1);
    cursor.add_column(columns_repeated);

    m_columns[cursor.column()] = std::make_unique<SheetColumn>(column_node);
  }

  for (auto row_node : m_node.children("table:table-row")) {
    const auto rows_repeated =
        row_node.attribute("table:number-rows-repeated").as_uint(1);
    cursor.add_row(rows_repeated);

    auto &row = m_rows[cursor.row()] = {};
    row.element = std::make_unique<SheetRow>(row_node);
    auto &cells = row.cells;

    for (auto cell_node : row_node.children("table:table-cell")) {
      const auto columns_repeated =
          cell_node.attribute("table:number-columns-repeated").as_uint(1);
      const auto colspan =
          cell_node.attribute("table:number-columns-spanned").as_uint(1);
      const auto rowspan =
          cell_node.attribute("table:number-rows-spanned").as_uint(1);
      cursor.add_cell(colspan, rowspan, columns_repeated);

      cells[cursor.column()] = std::make_unique<SheetCell>(cell_node);

      m_dimensions.columns = std::max(m_dimensions.columns, cursor.column());
      m_dimensions.rows = cursor.row();
    }
  }
}

std::string Sheet::name(const abstract::Document * /*document*/) const {
  return m_node.attribute("table:name").value();
}

TableDimensions
Sheet::dimensions(const abstract::Document * /*document*/) const {
  return m_dimensions;
}

TableDimensions
Sheet::content(const abstract::Document * /*document*/,
               const std::optional<TableDimensions> range) const {
  TableDimensions result;

  common::TableCursor cursor;
  for (auto row : m_node.children("table:table-row")) {
    const auto rows_repeated =
        row.attribute("table:number-rows-repeated").as_uint(1);
    cursor.add_row(rows_repeated);

    for (auto cell : row.children("table:table-cell")) {
      const auto columns_repeated =
          cell.attribute("table:number-columns-repeated").as_uint(1);
      const auto colspan =
          cell.attribute("table:number-columns-spanned").as_uint(1);
      const auto rowspan =
          cell.attribute("table:number-rows-spanned").as_uint(1);
      cursor.add_cell(colspan, rowspan, columns_repeated);

      const auto new_rows = cursor.row();
      const auto new_cols = std::max(result.columns, cursor.column());
      if (cell.first_child() &&
          (range && (new_rows < range->rows) && (new_cols < range->columns))) {
        result.rows = new_rows;
        result.columns = new_cols;
      }
    }
  }

  return result;
}

abstract::Element *Sheet::column(const abstract::Document * /*document*/,
                                 std::uint32_t column) const {
  if (auto it = m_columns.lower_bound(column); it != std::end(m_columns)) {
    return it->second.get();
  }
  throw std::runtime_error("column not found");
}

abstract::Element *Sheet::row(const abstract::Document * /*document*/,
                              std::uint32_t row) const {
  if (auto it = m_rows.lower_bound(row); it != std::end(m_rows)) {
    return it->second.element.get();
  }
  throw std::runtime_error("row not found");
}

abstract::Element *Sheet::cell(const abstract::Document * /*document*/,
                               std::uint32_t column, std::uint32_t row) const {
  if (auto row_it = m_rows.lower_bound(row); row_it != std::end(m_rows)) {
    auto &cells = row_it->second.cells;
    if (auto column_it = cells.lower_bound(column);
        column_it != std::end(cells)) {
      return column_it->second.get();
    }
    throw std::runtime_error("column not found");
  }
  throw std::runtime_error("row not found");
}

abstract::Element *
Sheet::first_shape(const abstract::Document * /*document*/) const {
  return {}; // TODO
}

TableStyle Sheet::style(const abstract::Document * /*document*/) const {
  return {}; // TODO
}

} // namespace odr::internal::odf
