#include <internal/common/table_cursor.h>
#include <internal/odf/odf_document.h>
#include <internal/odf/odf_table.h>
#include <odr/exceptions.h>

namespace odr::internal::odf {

class Table::PropertyRegistry final {
public:
  using Get = std::function<std::any(pugi::xml_node node)>;

  static PropertyRegistry &instance() {
    static PropertyRegistry instance;
    return instance;
  }

  void
  resolve_properties(const ElementType element, const pugi::xml_node node,
                     std::unordered_map<ElementProperty, std::any> &result) {
    auto it = m_registry.find(element);
    if (it == std::end(m_registry)) {
      return;
    }
    for (auto &&p : it->second) {
      auto value = p.second.get(node);
      if (value.has_value()) {
        result[p.first] = value;
      }
    }
  }

private:
  struct Entry {
    Get get;
  };

  std::unordered_map<ElementType, std::unordered_map<ElementProperty, Entry>>
      m_registry;

  PropertyRegistry() {
    static auto table_style_attribute = "table:style-name";

    default_register_(ElementType::TABLE_COLUMN, ElementProperty::STYLE_NAME,
                      table_style_attribute);
    default_register_(ElementType::TABLE_COLUMN,
                      ElementProperty::TABLE_COLUMN_DEFAULT_CELL_STYLE_NAME,
                      "table:default-cell-style-name");

    default_register_(ElementType::TABLE_ROW, ElementProperty::STYLE_NAME,
                      table_style_attribute);

    default_register_(ElementType::TABLE_CELL, ElementProperty::STYLE_NAME,
                      table_style_attribute);
    default_register_(ElementType::TABLE_CELL, ElementProperty::VALUE_TYPE,
                      "office:value-type");
  }

  void register_(const ElementType element, const ElementProperty property,
                 Get get) {
    m_registry[element][property].get = std::move(get);
  }

  void default_register_(const ElementType element,
                         const ElementProperty property,
                         const char *attribute_name) {
    register_(element, property, [attribute_name](const pugi::xml_node node) {
      auto attribute = node.attribute(attribute_name);
      if (!attribute) {
        return std::any();
      }
      return std::any(attribute.value());
    });
  }
};

Table::Table(OpenDocument &document, const pugi::xml_node node)
    : m_document{document} {
  register_(node);
}

void Table::register_(const pugi::xml_node node) {
  common::TableCursor cursor;
  std::optional<std::uint32_t> begin_col;
  std::optional<std::uint32_t> begin_row;
  std::optional<std::uint32_t> end_col;
  std::optional<std::uint32_t> end_row;

  for (auto column : node.children("table:table-column")) {
    const auto columns_repeated =
        column.attribute("table:number-columns-repeated").as_uint(1);

    Column new_column;
    new_column.node = column;
    m_columns[cursor.col()] = new_column;

    cursor.add_col(columns_repeated);
  }

  m_dimensions.columns = cursor.col();
  cursor = {};

  for (auto row : node.children("table:table-row")) {
    const auto rows_repeated =
        row.attribute("table:number-rows-repeated").as_uint(1);

    bool row_empty = true;

    for (std::uint32_t i = 0; i < rows_repeated; ++i) {
      Row new_row;
      new_row.node = row;

      for (auto cell : row.children("table:table-cell")) {
        const auto cells_repeated =
            cell.attribute("table:number-columns-repeated").as_uint(1);
        const auto colspan =
            cell.attribute("table:number-columns-spanned").as_uint(1);
        const auto rowspan =
            cell.attribute("table:number-rows-spanned").as_uint(1);

        bool cell_empty = true;

        for (std::uint32_t j = 0; j < cells_repeated; ++j) {
          // TODO parent?
          auto first_child = m_document.register_children_(cell, {}, {}).first;

          Cell new_cell;
          new_cell.node = cell;
          new_cell.first_child = first_child;
          new_cell.rowspan = rowspan;
          new_cell.colspan = colspan;
          new_row.cells[cursor.col()] = new_cell;

          if (first_child) {
            if (!begin_col || *begin_col > cursor.col()) {
              begin_col = cursor.col();
            }
            if (!begin_row || *begin_row > cursor.row()) {
              begin_row = cursor.row();
            }
            if (!end_col || *end_col < cursor.col()) {
              end_col = cursor.col();
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

  common::TablePosition begin{begin_row ? *begin_row : 0,
                              begin_col ? *begin_col : 0};
  common::TablePosition end{end_row ? *end_row + 1 : begin.row(),
                            end_col ? *end_col + 1 : begin.column()};
  m_content_bounds = {begin, end};
}

[[nodiscard]] std::shared_ptr<abstract::Document> Table::document() const {
  return m_document.shared_from_this();
}

[[nodiscard]] TableDimensions Table::dimensions() const { return m_dimensions; }

common::TableRange Table::content_bounds() const { return m_content_bounds; }

[[nodiscard]] ElementIdentifier
Table::cell_first_child(const std::uint32_t row,
                        const std::uint32_t column) const {
  auto c = cell_(row, column);
  if (c == nullptr) {
    return {};
  }
  return c->first_child;
}

TableDimensions Table::cell_span(const std::uint32_t row,
                                 const std::uint32_t column) const {
  auto c = cell_(row, column);
  if (c == nullptr) {
    return {};
  }
  return {c->rowspan, c->colspan};
}

std::unordered_map<ElementProperty, std::any>
Table::column_properties(const std::uint32_t column) const {
  auto c = column_(column);
  if (c == nullptr) {
    return {};
  }

  std::unordered_map<ElementProperty, std::any> result;

  PropertyRegistry::instance().resolve_properties(ElementType::TABLE_COLUMN,
                                                  c->node, result);

  if (auto style_name_it = result.find(ElementProperty::STYLE_NAME);
      style_name_it != std::end(result)) {
    auto style_name = std::any_cast<const char *>(style_name_it->second);
    auto style_properties =
        m_document.m_style.resolve_style(ElementType::TABLE_COLUMN, style_name);
    result.insert(std::begin(style_properties), std::end(style_properties));
  }

  return result;
}

std::unordered_map<ElementProperty, std::any>
Table::row_properties(const std::uint32_t row) const {
  auto r = row_(row);
  if (r == nullptr) {
    return {};
  }

  std::unordered_map<ElementProperty, std::any> result;

  PropertyRegistry::instance().resolve_properties(ElementType::TABLE_ROW,
                                                  r->node, result);

  if (auto style_name_it = result.find(ElementProperty::STYLE_NAME);
      style_name_it != std::end(result)) {
    auto style_name = std::any_cast<const char *>(style_name_it->second);
    auto style_properties =
        m_document.m_style.resolve_style(ElementType::TABLE_ROW, style_name);
    result.insert(std::begin(style_properties), std::end(style_properties));
  }

  return result;
}

std::unordered_map<ElementProperty, std::any>
Table::cell_properties(const std::uint32_t row,
                       const std::uint32_t column) const {
  auto c = cell_(row, column);
  if (c == nullptr) {
    return {};
  }

  std::unordered_map<ElementProperty, std::any> result;

  PropertyRegistry::instance().resolve_properties(ElementType::TABLE_CELL,
                                                  c->node, result);

  std::optional<std::string> style_name;
  if (auto it =
          result.find(ElementProperty::TABLE_COLUMN_DEFAULT_CELL_STYLE_NAME);
      it != std::end(result)) {
    style_name = std::any_cast<const char *>(it->second);
  }
  if (auto it = result.find(ElementProperty::STYLE_NAME);
      it != std::end(result)) {
    style_name = std::any_cast<const char *>(it->second);
  }
  if (style_name) {
    auto style_properties =
        m_document.m_style.resolve_style(ElementType::TABLE_CELL, *style_name);
    result.insert(std::begin(style_properties), std::end(style_properties));
  }

  return result;
}

void Table::update_column_properties(
    const std::uint32_t column,
    std::unordered_map<ElementProperty, std::any> properties) const {
  throw UnsupportedOperation(); // TODO
}

void Table::update_row_properties(
    const std::uint32_t row,
    std::unordered_map<ElementProperty, std::any> properties) const {
  throw UnsupportedOperation(); // TODO
}

void Table::update_cell_properties(
    const std::uint32_t row, const std::uint32_t column,
    std::unordered_map<ElementProperty, std::any> properties) const {
  throw UnsupportedOperation(); // TODO
}

void Table::resize(const std::uint32_t rows,
                   const std::uint32_t columns) const {
  throw UnsupportedOperation(); // TODO
}

void Table::decouple_cell(const std::uint32_t row,
                          const std::uint32_t column) const {
  throw UnsupportedOperation(); // TODO
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
