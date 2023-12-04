#include <odr/internal/odf/odf_spreadsheet.hpp>

#include <odr/internal/common/table_cursor.hpp>
#include <odr/internal/util/map_util.hpp>

#include <cstring>
#include <stdexcept>

namespace odr::internal::odf {

class SheetColumn final : public Element, public abstract::SheetColumn {
public:
  using Element::Element;

  [[nodiscard]] TableColumnStyle style(const abstract::Document *document,
                                       abstract::Sheet *,
                                       std::uint32_t /*column*/) const final {
    return partial_style(document).table_column_style;
  }
};

class SheetRow final : public Element, public abstract::SheetRow {
public:
  using Element::Element;

  [[nodiscard]] TableRowStyle style(const abstract::Document *document,
                                    abstract::Sheet *,
                                    std::uint32_t /*column*/) const final {
    return partial_style(document).table_row_style;
  }
};

class SheetCell final : public Element, public abstract::SheetCell {
public:
  using Element::Element;

  [[nodiscard]] bool is_covered(const abstract::Document *, abstract::Sheet *,
                                std::uint32_t /*column*/,
                                std::uint32_t /*row*/) const final {
    return std::strcmp(m_node.name(), "table:covered-table-cell") == 0;
  }

  [[nodiscard]] TableDimensions span(const abstract::Document *,
                                     abstract::Sheet *,
                                     std::uint32_t /*column*/,
                                     std::uint32_t /*row*/) const final {
    return {m_node.attribute("table:number-rows-spanned").as_uint(1),
            m_node.attribute("table:number-columns-spanned").as_uint(1)};
  }

  [[nodiscard]] ValueType value_type(const abstract::Document *,
                                     abstract::Sheet *,
                                     std::uint32_t /*column*/,
                                     std::uint32_t /*row*/) const final {
    auto value_type = m_node.attribute("office:value-type").value();
    if (std::strcmp("float", value_type) == 0) {
      return ValueType::float_number;
    }
    return ValueType::string;
  }

  [[nodiscard]] TableCellStyle style(const abstract::Document *document,
                                     abstract::Sheet *sheet,
                                     std::uint32_t column,
                                     std::uint32_t row) const final {
    auto style_name = style_name_(document);
    if (style_name == nullptr) {
      auto row_element = sheet->row(document, row);
      // TODO should this ever be null?
      if (row_element != nullptr) {
        auto row_node = dynamic_cast<SheetRow *>(row_element)->m_node;
        if (auto attr = row_node.attribute("table:default-cell-style-name")) {
          style_name = attr.value();
        }
      }
    }
    if (style_name == nullptr) {
      auto column_element = sheet->column(document, column);
      // TODO should this ever be null?
      if (column_element != nullptr) {
        auto column_node = dynamic_cast<SheetColumn *>(column_element)->m_node;
        if (auto attr =
                column_node.attribute("table:default-cell-style-name")) {
          style_name = attr.value();
        }
      }
    }
    if (style_name != nullptr) {
      if (auto style = style_(document)->style(style_name)) {
        return style->resolved().table_cell_style;
      }
    }
    return {};
  }
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

abstract::SheetColumn *Sheet::column(const abstract::Document *,
                                     std::uint32_t column) const {
  if (auto it = util::map::lookup_greater_than(m_columns, column);
      it != std::end(m_columns)) {
    return dynamic_cast<abstract::SheetColumn *>(it->second);
  }
  return nullptr;
}

abstract::SheetRow *Sheet::row(const abstract::Document *,
                               std::uint32_t row) const {
  if (auto it = util::map::lookup_greater_than(m_rows, row);
      it != std::end(m_rows)) {
    return it->second.element;
  }
  return nullptr;
}

abstract::SheetCell *Sheet::cell(const abstract::Document *,
                                 std::uint32_t column,
                                 std::uint32_t row) const {
  if (auto row_it = util::map::lookup_greater_than(m_rows, row);
      row_it != std::end(m_rows)) {
    auto &cells = row_it->second.cells;
    if (auto column_it = util::map::lookup_greater_than(cells, column);
        column_it != std::end(cells)) {
      return column_it->second;
    }
    return nullptr;
  }
  return nullptr;
}

abstract::Element *Sheet::first_shape(const abstract::Document *) const {
  return m_first_shape;
}

TableStyle Sheet::style(const abstract::Document *document) const {
  return partial_style(document).table_style;
}

void Sheet::init_column_(std::uint32_t column, std::uint32_t repeated,
                         SheetColumn *element) {
  m_columns[column + repeated] = dynamic_cast<abstract::SheetColumn *>(element);
}

void Sheet::init_row_(std::uint32_t row, std::uint32_t repeated,
                      SheetRow *element) {
  m_rows[row + repeated].element = dynamic_cast<abstract::SheetRow *>(element);
}

void Sheet::init_cell_(std::uint32_t column, std::uint32_t row,
                       std::uint32_t columns_repeated,
                       std::uint32_t rows_repeated, SheetCell *element) {
  m_rows[row + rows_repeated].cells[column + columns_repeated] =
      dynamic_cast<abstract::SheetCell *>(element);
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

    auto [column, _] = parse_element_tree<SheetColumn>(document, column_node);

    sheet.init_column_(cursor.column(), columns_repeated, column);

    cursor.add_column(columns_repeated);
  }

  dimensions.columns = cursor.column();
  cursor = {};

  for (auto row_node : node.children("table:table-row")) {
    const auto rows_repeated =
        row_node.attribute("table:number-rows-repeated").as_uint(1);

    auto [row, _] = parse_element_tree<SheetRow>(document, row_node);

    sheet.init_row_(cursor.row(), rows_repeated, row);

    for (auto cell_node : row_node.children("table:table-cell")) {
      const auto columns_repeated =
          cell_node.attribute("table:number-columns-repeated").as_uint(1);
      const auto colspan =
          cell_node.attribute("table:number-columns-spanned").as_uint(1);
      const auto rowspan =
          cell_node.attribute("table:number-rows-spanned").as_uint(1);

      auto [cell, _] = parse_element_tree<SheetCell>(document, cell_node);

      sheet.init_cell_(cursor.column(), cursor.row(), columns_repeated,
                       rows_repeated, cell);

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
