#include <odr/internal/odf/odf_spreadsheet.hpp>

#include <odr/internal/common/table_cursor.hpp>
#include <odr/internal/util/map_util.hpp>

#include <cstring>
#include <stdexcept>

namespace odr::internal::odf {

void SheetIndex::init_column(const std::uint32_t column,
                             const std::uint32_t repeated,
                             const pugi::xml_node element) {
  columns[column + repeated] = element;
}

void SheetIndex::init_row(const std::uint32_t row, const std::uint32_t repeated,
                          const pugi::xml_node element) {
  rows[row + repeated].row = element;
}

void SheetIndex::init_cell(const std::uint32_t column, const std::uint32_t row,
                           const std::uint32_t columns_repeated,
                           const std::uint32_t rows_repeated,
                           const pugi::xml_node element) {
  rows[row + rows_repeated].cells[column + columns_repeated] = element;
}

pugi::xml_node SheetIndex::column(const std::uint32_t column) const {
  if (const auto it = util::map::lookup_greater_than(columns, column);
      it != std::end(columns)) {
    return it->second;
  }
  return {};
}

pugi::xml_node SheetIndex::row(const std::uint32_t row) const {
  if (const auto it = util::map::lookup_greater_than(rows, row);
      it != std::end(rows)) {
    return it->second.row;
  }
  return {};
}

pugi::xml_node SheetIndex::cell(const std::uint32_t column,
                                const std::uint32_t row) const {
  if (const auto row_it = util::map::lookup_greater_than(rows, row);
      row_it != std::end(rows)) {
    const auto &cells = row_it->second.cells;
    if (const auto cell_it = util::map::lookup_greater_than(cells, column);
        cell_it != std::end(cells)) {
      return cell_it->second;
    }
  }
  return {};
}

class SheetCell final : public Element, public abstract::SheetCell {
public:
  SheetCell(const pugi::xml_node node, const std::uint32_t column,
            const std::uint32_t row, const bool is_repeated)
      : Element(node), m_column{column}, m_row{row},
        m_is_repeated{is_repeated} {}

  [[nodiscard]] bool is_covered(const abstract::Document *) const override {
    return std::strcmp(m_node.name(), "table:covered-table-cell") == 0;
  }

  [[nodiscard]] TableDimensions
  span(const abstract::Document *) const override {
    return {m_node.attribute("table:number-rows-spanned").as_uint(1),
            m_node.attribute("table:number-columns-spanned").as_uint(1)};
  }

  [[nodiscard]] ValueType
  value_type(const abstract::Document *) const override {
    if (const char *value_type = m_node.attribute("office:value-type").value();
        std::strcmp("float", value_type) == 0) {
      return ValueType::float_number;
    }
    return ValueType::string;
  }

  ResolvedStyle
  partial_style(const abstract::Document *document) const override {
    const auto *sheet = dynamic_cast<const Sheet *>(parent(document));
    return sheet->cell_style_(document, m_column, m_row);
  }

  ResolvedStyle
  intermediate_style(const abstract::Document *document) const override {
    return partial_style(document);
  }

  [[nodiscard]] TableCellStyle
  style(const abstract::Document *document) const override {
    return intermediate_style(document).table_cell_style;
  }

  bool is_editable(const abstract::Document *) const override {
    return !m_is_repeated;
  }

private:
  std::uint32_t m_column{};
  std::uint32_t m_row{};
  bool m_is_repeated{};
};

std::string Sheet::name(const abstract::Document *) const {
  return m_node.attribute("table:name").value();
}

TableDimensions Sheet::dimensions(const abstract::Document *) const {
  return m_index.dimensions;
}

TableDimensions
Sheet::content(const abstract::Document *,
               const std::optional<TableDimensions> range) const {
  TableDimensions result;

  TableCursor cursor;
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

      const std::uint32_t new_rows = cursor.row();
      if (const std::uint32_t new_cols =
              std::max(result.columns, cursor.column());
          cell.first_child() && range && new_rows < range->rows &&
          new_cols < range->columns) {
        result.rows = new_rows;
        result.columns = new_cols;
      }
    }
  }

  return result;
}

ResolvedStyle Sheet::cell_style_(const abstract::Document *document,
                                 const std::uint32_t column,
                                 const std::uint32_t row) const {
  const char *style_name = nullptr;

  const pugi::xml_node cell_node = m_index.cell(column, row);
  if (const pugi::xml_attribute attr =
          cell_node.attribute("table:style-name")) {
    style_name = attr.value();
  }

  if (style_name == nullptr) {
    const pugi::xml_node row_node = m_index.row(row);
    if (const pugi::xml_attribute attr =
            row_node.attribute("table:default-cell-style-name")) {
      style_name = attr.value();
    }
  }
  if (style_name == nullptr) {
    const pugi::xml_node column_node = m_index.column(column);
    if (const pugi::xml_attribute attr =
            column_node.attribute("table:default-cell-style-name")) {
      style_name = attr.value();
    }
  }

  if (style_name != nullptr) {
    if (const Style *style = style_(document)->style(style_name);
        style != nullptr) {
      return style->resolved();
    }
  }

  return {};
}

abstract::SheetCell *Sheet::cell(const abstract::Document *,
                                 const std::uint32_t column,
                                 const std::uint32_t row) const {
  if (const auto cell_it = m_cells.find({column, row});
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
                                     const std::uint32_t column) const {
  if (const pugi::xml_node column_node = m_index.column(column); column_node) {
    if (const pugi::xml_attribute attr =
            column_node.attribute("table:style-name")) {
      if (const Style *style = style_(document)->style(attr.value());
          style != nullptr) {
        return style->resolved().table_column_style;
      }
    }
  }
  return {};
}

TableRowStyle Sheet::row_style(const abstract::Document *document,
                               const std::uint32_t row) const {
  if (const pugi::xml_node column_node = m_index.row(row); column_node) {
    if (const pugi::xml_attribute attr =
            column_node.attribute("table:style-name")) {
      if (const Style *style = style_(document)->style(attr.value());
          style != nullptr) {
        return style->resolved().table_row_style;
      }
    }
  }
  return {};
}

TableCellStyle Sheet::cell_style(const abstract::Document *document,
                                 const std::uint32_t column,
                                 const std::uint32_t row) const {
  return cell_style_(document, column, row).table_cell_style;
}

void Sheet::init_column_(const std::uint32_t column,
                         const std::uint32_t repeated,
                         const pugi::xml_node element) {
  m_index.init_column(column, repeated, element);
}

void Sheet::init_row_(const std::uint32_t row, const std::uint32_t repeated,
                      const pugi::xml_node element) {
  m_index.init_row(row, repeated, element);
}

void Sheet::init_cell_(const std::uint32_t column, const std::uint32_t row,
                       const std::uint32_t columns_repeated,
                       const std::uint32_t rows_repeated,
                       const pugi::xml_node element) {
  m_index.init_cell(column, row, columns_repeated, rows_repeated, element);
}

void Sheet::init_cell_element_(const std::uint32_t column,
                               const std::uint32_t row, SheetCell *element) {
  m_cells[{column, row}] = element;
  element->m_parent = this;
}

void Sheet::init_dimensions_(const TableDimensions dimensions) {
  m_index.dimensions = dimensions;
}

void Sheet::append_shape_(Element *shape) {
  shape->m_previous_sibling = m_last_shape;
  shape->m_parent = this;
  if (m_last_shape == nullptr) {
    m_first_shape = shape;
  } else {
    m_last_shape->m_next_sibling = shape;
  }
  m_last_shape = shape;
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
  TableCursor cursor;

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
      const bool is_repeated = columns_repeated > 1 || rows_repeated > 1;

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
                cursor.row() + row_repeat, is_repeated);
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

  for (const pugi::xml_node shape_node :
       node.child("table:shapes").children()) {
    if (auto [shape, _] = parse_any_element_tree(document, shape_node);
        shape != nullptr) {
      sheet.append_shape_(shape);
    }
  }

  return std::make_tuple(&sheet, node.next_sibling());
}

} // namespace odr::internal
