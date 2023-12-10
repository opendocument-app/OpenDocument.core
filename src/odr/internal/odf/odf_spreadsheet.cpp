#include <odr/internal/odf/odf_spreadsheet.hpp>

#include <odr/internal/common/table_cursor.hpp>
#include <odr/internal/util/map_util.hpp>

#include <cstring>
#include <stdexcept>

namespace odr::internal::odf {

class SheetCell final : public Element, public abstract::SheetCell {
public:
  SheetCell(pugi::xml_node node, std::uint32_t column, std::uint32_t row)
      : Element(node), m_column{column}, m_row{row} {}

  [[nodiscard]] bool is_covered(const abstract::Document *) const final {
    return std::strcmp(m_node.name(), "table:covered-table-cell") == 0;
  }

  [[nodiscard]] TableDimensions span(const abstract::Document *) const final {
    return {m_node.attribute("table:number-rows-spanned").as_uint(1),
            m_node.attribute("table:number-columns-spanned").as_uint(1)};
  }

  [[nodiscard]] ValueType value_type(const abstract::Document *) const final {
    auto value_type = m_node.attribute("office:value-type").value();
    if (std::strcmp("float", value_type) == 0) {
      return ValueType::float_number;
    }
    return ValueType::string;
  }

  common::ResolvedStyle
  partial_style(const abstract::Document *document) const final {
    auto sheet = dynamic_cast<const Sheet *>(parent(document));
    return sheet->cell_style_(document, m_column, m_row);
  }

  common::ResolvedStyle
  intermediate_style(const abstract::Document *document) const final {
    return partial_style(document);
  }

  [[nodiscard]] TableCellStyle
  style(const abstract::Document *document) const final {
    return intermediate_style(document).table_cell_style;
  }

private:
  std::uint32_t m_column;
  std::uint32_t m_row;
};

std::string Sheet::name(const abstract::Document *) const {
  return m_node.attribute("table:name").value();
}

TableDimensions Sheet::dimensions(const abstract::Document *) const {
  return m_dimensions;
}

TableDimensions
Sheet::content(const abstract::Document *,
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

pugi::xml_node Sheet::column_(std::uint32_t column) const {
  if (auto it = util::map::lookup_greater_than(m_columns, column);
      it != std::end(m_columns)) {
    return it->second;
  }
  return {};
}

pugi::xml_node Sheet::row_(std::uint32_t row) const {
  if (auto it = util::map::lookup_greater_than(m_rows, row);
      it != std::end(m_rows)) {
    return it->second.row;
  }
  return {};
}

common::ResolvedStyle Sheet::cell_style_(const abstract::Document *document,
                                         std::uint32_t column,
                                         std::uint32_t row) const {
  auto it = m_cells.find({column, row});
  if (it == std::end(m_cells)) {
    return common::ResolvedStyle();
  }

  const char *style_name = nullptr;

  auto cell_node = it->second->m_node;
  if (auto attr = cell_node.attribute("table:style-name")) {
    style_name = attr.value();
  }

  if (style_name == nullptr) {
    auto row_node = row_(row);
    if (auto attr = row_node.attribute("table:default-cell-style-name")) {
      style_name = attr.value();
    }
  }
  if (style_name == nullptr) {
    auto column_node = column_(column);
    if (auto attr = column_node.attribute("table:default-cell-style-name")) {
      style_name = attr.value();
    }
  }

  if (style_name == nullptr) {
    return common::ResolvedStyle();
  }
  auto style = style_(document)->style(style_name);
  if (style == nullptr) {
    return common::ResolvedStyle();
  }
  return style->resolved();
}

abstract::SheetCell *Sheet::cell(const abstract::Document *,
                                 std::uint32_t column,
                                 std::uint32_t row) const {
  if (auto cell_it = m_cells.find({column, row});
      cell_it != std::end(m_cells)) {
    return cell_it->second;
  }
  return nullptr;
}

abstract::Element *Sheet::first_shape(const abstract::Document *) const {
  return m_first_shape;
}

TableStyle Sheet::style(const abstract::Document *document) const {
  return partial_style(document).table_style;
}

TableColumnStyle Sheet::column_style(const abstract::Document *document,
                                     std::uint32_t column) const {
  auto column_node = column_(column);
  if (column_node) {
    if (auto attr = column_node.attribute("table:style-name")) {
      auto style = style_(document)->style(attr.value());
      if (style != nullptr) {
        return style->resolved().table_column_style;
      }
    }
  }
  return TableColumnStyle();
}

TableRowStyle Sheet::row_style(const abstract::Document *document,
                               std::uint32_t row) const {
  auto column_node = row_(row);
  if (column_node) {
    if (auto attr = column_node.attribute("table:style-name")) {
      auto style = style_(document)->style(attr.value());
      if (style != nullptr) {
        return style->resolved().table_row_style;
      }
    }
  }
  return TableRowStyle();
}

TableCellStyle Sheet::cell_style(const abstract::Document *document,
                                 std::uint32_t column,
                                 std::uint32_t row) const {
  return cell_style_(document, column, row).table_cell_style;
}

void Sheet::init_column_(std::uint32_t column, std::uint32_t repeated,
                         pugi::xml_node element) {
  m_columns[column + repeated] = element;
}

void Sheet::init_row_(std::uint32_t row, std::uint32_t repeated,
                      pugi::xml_node element) {
  m_rows[row + repeated].row = element;
}

void Sheet::init_cell_(std::uint32_t column, std::uint32_t row,
                       std::uint32_t columns_repeated,
                       std::uint32_t rows_repeated, pugi::xml_node element) {
  m_rows[row + rows_repeated].cells[column + columns_repeated] = element;
}

void Sheet::init_cell_element_(std::uint32_t column, std::uint32_t row,
                               SheetCell *element) {
  m_cells[{column, row}] = element;
  element->m_parent = this;
}

void Sheet::init_dimensions_(TableDimensions dimensions) {
  m_dimensions = dimensions;
}

} // namespace odr::internal::odf

namespace odr::internal {

template <>
std::tuple<odf::Sheet *, pugi::xml_node>
odf::parse_element_tree<odf::Sheet>(Document &document, pugi::xml_node node) {
  if (!node) {
    return std::make_tuple(nullptr, pugi::xml_node());
  }

  auto sheet_unique = std::make_unique<Sheet>(node);
  auto &sheet = *sheet_unique;
  document.register_element_(std::move(sheet_unique));

  TableDimensions dimensions;
  common::TableCursor cursor;

  for (auto column_node : node.children("table:table-column")) {
    const auto columns_repeated =
        column_node.attribute("table:number-columns-repeated").as_uint(1);

    sheet.init_column_(cursor.column(), columns_repeated, column_node);

    cursor.add_column(columns_repeated);
  }

  dimensions.columns = cursor.column();
  cursor = {};

  for (auto row_node : node.children("table:table-row")) {
    const auto rows_repeated =
        row_node.attribute("table:number-rows-repeated").as_uint(1);

    sheet.init_row_(cursor.row(), rows_repeated, row_node);

    // TODO covered cells
    for (auto cell_node : row_node.children("table:table-cell")) {
      const auto columns_repeated =
          cell_node.attribute("table:number-columns-repeated").as_uint(1);
      const auto colspan =
          cell_node.attribute("table:number-columns-spanned").as_uint(1);
      const auto rowspan =
          cell_node.attribute("table:number-rows-spanned").as_uint(1);

      sheet.init_cell_(cursor.column(), cursor.row(), columns_repeated,
                       rows_repeated, cell_node);

      bool empty = false;
      if (!cell_node.first_child()) {
        empty = true;
      }

      if (!empty) {
        for (std::uint32_t row_repeat = 0; row_repeat < rows_repeated;
             ++row_repeat) {
          for (std::uint32_t column_repeat = 0;
               column_repeat < columns_repeated; ++column_repeat) {
            auto [cell, _] = parse_element_tree<SheetCell>(
                document, cell_node, cursor.column() + column_repeat,
                cursor.row() + row_repeat);
            sheet.init_cell_element_(cursor.column() + column_repeat,
                                     cursor.row() + row_repeat, cell);
          }
        }
      }

      cursor.add_cell(colspan, rowspan, columns_repeated);
    }

    cursor.add_row(rows_repeated);
  }

  dimensions.rows = cursor.row();

  sheet.init_dimensions_(dimensions);

  // TODO shapes

  return std::make_tuple(&sheet, node.next_sibling());
}

} // namespace odr::internal
