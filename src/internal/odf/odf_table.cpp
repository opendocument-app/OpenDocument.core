#include <internal/common/table_cursor.h>
#include <internal/odf/odf_document.h>
#include <internal/odf/odf_table.h>
#include <odr/exceptions.h>

namespace odr::internal::odf {

Table::Table(OpenDocument &document, odf::TableElement element)
    : m_document{document} {
  register_(element);
}

void Table::register_(const odf::TableElement element) {
  common::TableCursor cursor;
  std::optional<std::uint32_t> begin_column;
  std::optional<std::uint32_t> begin_row;
  std::optional<std::uint32_t> end_column;
  std::optional<std::uint32_t> end_row;

  for (auto column : element.columns()) {
    const auto columns_repeated =
        column.xml_node().attribute("table:number-columns-repeated").as_uint(1);

    Column new_column;
    new_column.element = TableColumnElement(column);
    m_columns[cursor.column()] = new_column;

    cursor.add_column(columns_repeated);
  }

  m_dimensions.columns = cursor.column();
  cursor = {};

  for (auto row : element.rows()) {
    const auto rows_repeated =
        row.xml_node().attribute("table:number-rows-repeated").as_uint(1);

    bool row_empty = true;

    for (std::uint32_t i = 0; i < rows_repeated; ++i) {
      Row new_row;
      new_row.element = TableRowElement(row);

      for (auto cell : row.cells()) {
        // TODO performance - fetch all attributes at once
        const auto cells_repeated =
            cell.xml_node()
                .attribute("table:number-columns-repeated")
                .as_uint(1);
        const auto colspan = cell.xml_node()
                                 .attribute("table:number-columns-spanned")
                                 .as_uint(1);
        const auto rowspan =
            cell.xml_node().attribute("table:number-rows-spanned").as_uint(1);

        bool cell_empty = true;

        for (std::uint32_t j = 0; j < cells_repeated; ++j) {
          // TODO parent?
          // TODO
          // auto first_child =
          //    m_document.register_children_(cell.element(), {}, {}).first;

          Cell new_cell;
          new_cell.element = TableCellElement(cell);
          new_cell.rowspan = rowspan;
          new_cell.colspan = colspan;
          new_row.cells[cursor.column()] = new_cell;

          if (new_cell.element.first_child()) {
            if (!begin_column || *begin_column > cursor.column()) {
              begin_column = cursor.column();
            }
            if (!begin_row || *begin_row > cursor.row()) {
              begin_row = cursor.row();
            }
            // TODO consider span?
            if (!end_column || *end_column < cursor.column()) {
              end_column = cursor.column();
            }
            if (!end_row || *end_row < cursor.row()) {
              end_row = cursor.row();
            }

            cell_empty = false;
            row_empty = false;
          }

          if (cell_empty) {
            cursor.add_cell(colspan, rowspan, cells_repeated);
            break;
          }

          cursor.add_cell(colspan, rowspan);
        }
      }

      m_rows[cursor.row()] = new_row;

      if (row_empty) {
        cursor.add_row(rows_repeated);
        break;
      }

      cursor.add_row();
    }
  }

  m_dimensions.rows = cursor.row();

  common::TablePosition begin{begin_row.value_or(0), begin_column.value_or(0)};
  common::TablePosition end{end_row ? *end_row + 1 : begin.row(),
                            end_column ? *end_column + 1 : begin.column()};
  m_content_bounds = {begin, end};
}

TableDimensions Table::dimensions() const { return m_dimensions; }

common::TableRange Table::content_bounds() const { return m_content_bounds; }

common::TableRange
Table::content_bounds(const common::TableRange within) const {
  std::optional<std::uint32_t> begin_column;
  std::optional<std::uint32_t> begin_row;
  std::optional<std::uint32_t> end_column;
  std::optional<std::uint32_t> end_row;

  for (std::uint32_t row = within.from().row(); row < within.to().row();
       ++row) {
    for (std::uint32_t column = within.from().column();
         column < within.to().column(); ++column) {
      auto first_child = cell(row, column).first_child();
      if (!first_child) {
        continue;
      }

      if (!begin_column || *begin_column > column) {
        begin_column = column;
      }
      if (!begin_row || *begin_row > row) {
        begin_row = row;
      }
      // TODO consider span?
      if (!end_column || *end_column < column) {
        end_column = column;
      }
      if (!end_row || *end_row < row) {
        end_row = row;
      }
    }
  }

  common::TablePosition begin{begin_row.value_or(within.from().row()),
                              begin_column.value_or(within.from().column())};
  common::TablePosition end{end_row ? *end_row + 1 : begin.row(),
                            end_column ? *end_column + 1 : begin.column()};
  return {begin, end};
}

odr::Element Table::column(const std::uint32_t /*column*/) const {
  return {}; // TODO
}

odr::Element Table::row(const std::uint32_t /*row*/) const {
  return {}; // TODO
}

odr::Element Table::cell(const std::uint32_t /*row*/,
                         const std::uint32_t /*column*/) const {
  return {}; // TODO
}

const Table::Column *Table::column_(const std::uint32_t column) const {
  if (m_columns.empty()) {
    return nullptr;
  }

  auto it = m_columns.upper_bound(column);
  if (it != std::begin(m_columns)) {
    --it;
  }
  return &it->second;
}

const Table::Row *Table::row_(const std::uint32_t row) const {
  if (m_rows.empty()) {
    return nullptr;
  }

  auto it = m_rows.upper_bound(row);
  if (it != std::begin(m_rows)) {
    --it;
  }
  return &it->second;
}

const Table::Cell *Table::cell_(const std::uint32_t row,
                                const std::uint32_t column) const {
  auto row_ptr = row_(row);
  if (row_ptr == nullptr) {
    return nullptr;
  }

  auto &&cells = row_ptr->cells;
  if (cells.empty()) {
    return nullptr;
  }

  auto it = cells.upper_bound(column);
  if (it != std::begin(cells)) {
    --it;
  }
  return &it->second;
}

} // namespace odr::internal::odf
