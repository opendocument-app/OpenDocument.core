#include <internal/common/table_cursor.h>
#include <internal/odf/odf_document.h>
#include <internal/odf/odf_sheet.h>
#include <odr/exceptions.h>

namespace odr::internal::odf {

namespace {
class SheetColumn : public abstract::Element {
public:
  SheetColumn(OpenDocumentSpreadsheet *document, pugi::xml_node node,
              const std::uint32_t column)
      : m_document{document}, m_node{node}, m_column{column} {}

  bool operator==(const abstract::Element &rhs) const final {
    return m_node == static_cast<const SheetColumn &>(rhs).m_node;
  }

  bool operator!=(const abstract::Element &rhs) const final {
    return m_node != static_cast<const SheetColumn &>(rhs).m_node;
  }

  [[nodiscard]] ElementType type() const final {
    return ElementType::TABLE_COLUMN;
  }

  [[nodiscard]] odr::Element copy() const override {
    return odr::Element(*this);
  }

  [[nodiscard]] odr::Element parent() const override {
    return {}; // TODO return sheet
  }

  [[nodiscard]] odr::Element first_child() const override { return {}; }

  [[nodiscard]] odr::Element previous_sibling() const override {
    return {}; // TODO
  }

  [[nodiscard]] odr::Element next_sibling() const override {
    return {}; // TODO
  }

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties() const override {
    static std::unordered_map<std::string, ElementProperty> properties{
        {"table:style-name", ElementProperty::STYLE_NAME},
        {"table:default-cell-style-name",
         ElementProperty::TABLE_COLUMN_DEFAULT_CELL_STYLE_NAME},
    };
    return fetch_properties(properties, m_node);
  }

private:
  OpenDocumentSpreadsheet *m_document;
  pugi::xml_node m_node;
  std::uint32_t m_column;
};

class SheetRow : public abstract::Element {
public:
  SheetRow(OpenDocumentSpreadsheet *document, pugi::xml_node node,
           const std::uint32_t row)
      : m_document{document}, m_node{node}, m_row{row} {}

  bool operator==(const abstract::Element &rhs) const final {
    return m_node == static_cast<const SheetRow &>(rhs).m_node;
  }

  bool operator!=(const abstract::Element &rhs) const final {
    return m_node != static_cast<const SheetRow &>(rhs).m_node;
  }

  [[nodiscard]] ElementType type() const final {
    return ElementType::TABLE_ROW;
  }

  [[nodiscard]] odr::Element copy() const override {
    return odr::Element(*this);
  }

  [[nodiscard]] odr::Element parent() const override {
    return {}; // TODO return sheet
  }

  [[nodiscard]] odr::Element first_child() const override { return {}; }

  [[nodiscard]] odr::Element previous_sibling() const override {
    return {}; // TODO
  }

  [[nodiscard]] odr::Element next_sibling() const override {
    return {}; // TODO
  }

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties() const override {
    static std::unordered_map<std::string, ElementProperty> properties{
        {"table:style-name", ElementProperty::STYLE_NAME},
        {"table:number-rows-repeated", ElementProperty::ROWS_REPEATED},
    };
    return fetch_properties(properties, m_node);
  }

private:
  OpenDocumentSpreadsheet *m_document;
  pugi::xml_node m_node;
  std::uint32_t m_row;
};

class SheetCell : public abstract::Element {
public:
  SheetCell(OpenDocumentSpreadsheet *document, pugi::xml_node node,
            const std::uint32_t row, const std::uint32_t column)
      : m_document{document}, m_node{node}, m_row{row}, m_column{column} {}

  bool operator==(const abstract::Element &rhs) const final {
    return m_node == static_cast<const SheetCell &>(rhs).m_node;
  }

  bool operator!=(const abstract::Element &rhs) const final {
    return m_node != static_cast<const SheetCell &>(rhs).m_node;
  }

  [[nodiscard]] ElementType type() const final {
    return ElementType::TABLE_CELL;
  }

  [[nodiscard]] odr::Element copy() const override {
    return odr::Element(*this);
  }

  [[nodiscard]] odr::Element parent() const override {
    return {}; // TODO return sheet
  }

  [[nodiscard]] odr::Element first_child() const override {
    return {}; // TODO
    // return create_default_first_child_element(m_node, m_document);
  }

  [[nodiscard]] odr::Element previous_sibling() const override {
    return {}; // TODO
  }

  [[nodiscard]] odr::Element next_sibling() const override {
    return {}; // TODO
  }

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties() const override {
    static std::unordered_map<std::string, ElementProperty> properties{
        {"table:style-name", ElementProperty::STYLE_NAME},
        {"table:number-columns-spanned", ElementProperty::COLUMN_SPAN},
        {"table:number-rows-spanned", ElementProperty::ROW_SPAN},
        {"table:number-columns-repeated", ElementProperty::COLUMNS_REPEATED},
        {"office:value-type", ElementProperty::VALUE_TYPE},
    };
    return fetch_properties(properties, m_node);
  }

private:
  OpenDocumentSpreadsheet *m_document;
  pugi::xml_node m_node;
  std::uint32_t m_row;
  std::uint32_t m_column;
};
} // namespace

Sheet::Sheet(pugi::xml_node node, OpenDocumentSpreadsheet *document)
    : m_document{document} {
  register_(node);
}

void Sheet::register_(const pugi::xml_node node) {
  common::TableCursor cursor;
  std::optional<std::uint32_t> begin_column;
  std::optional<std::uint32_t> begin_row;
  std::optional<std::uint32_t> end_column;
  std::optional<std::uint32_t> end_row;

  for (auto column : node.children("table:table-column")) {
    const auto columns_repeated =
        column.attribute("table:number-columns-repeated").as_uint(1);

    Column new_column;
    new_column.node = node;
    m_columns[cursor.column()] = new_column;

    cursor.add_column(columns_repeated);
  }

  m_dimensions.columns = cursor.column();
  cursor = {};

  for (auto row : node.children("table:table-row")) {
    const auto rows_repeated =
        row.attribute("table:number-rows-repeated").as_uint(1);

    bool row_empty = true;

    for (std::uint32_t i = 0; i < rows_repeated; ++i) {
      Row new_row;
      new_row.node = row;

      for (auto cell : row.children("table:table-cell")) {
        // TODO performance - fetch all attributes at once
        const auto cells_repeated =
            cell.attribute("table:number-columns-repeated").as_uint(1);
        const auto colspan =
            cell.attribute("table:number-columns-spanned").as_uint(1);
        const auto rowspan =
            cell.attribute("table:number-rows-spanned").as_uint(1);

        bool cell_empty = true;

        for (std::uint32_t j = 0; j < cells_repeated; ++j) {
          // TODO parent?
          // TODO
          // auto first_child =
          //    m_document.register_children_(cell.element(), {}, {}).first;

          Cell new_cell;
          new_cell.node = cell;
          new_cell.rowspan = rowspan;
          new_cell.colspan = colspan;
          new_row.cells[cursor.column()] = new_cell;

          if (new_cell.node.first_child()) {
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

TableDimensions Sheet::dimensions() const { return m_dimensions; }

common::TableRange Sheet::content_bounds() const { return m_content_bounds; }

common::TableRange
Sheet::content_bounds(const common::TableRange within) const {
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

odr::Element Sheet::column(const std::uint32_t /*column*/) const {
  return {}; // TODO
}

odr::Element Sheet::row(const std::uint32_t /*row*/) const {
  return {}; // TODO
}

odr::Element Sheet::cell(const std::uint32_t /*row*/,
                         const std::uint32_t /*column*/) const {
  return {}; // TODO
}

const Sheet::Column *Sheet::column_(const std::uint32_t column) const {
  if (m_columns.empty()) {
    return nullptr;
  }

  auto it = m_columns.upper_bound(column);
  if (it != std::begin(m_columns)) {
    --it;
  }
  return &it->second;
}

const Sheet::Row *Sheet::row_(const std::uint32_t row) const {
  if (m_rows.empty()) {
    return nullptr;
  }

  auto it = m_rows.upper_bound(row);
  if (it != std::begin(m_rows)) {
    --it;
  }
  return &it->second;
}

const Sheet::Cell *Sheet::cell_(const std::uint32_t row,
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
