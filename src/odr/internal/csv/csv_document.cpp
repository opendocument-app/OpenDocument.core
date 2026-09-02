#include <odr/internal/csv/csv_document.hpp>

#include <odr/document_element.hpp>
#include <odr/document_path.hpp>
#include <odr/exceptions.hpp>
#include <odr/style.hpp>

#include <odr/internal/abstract/document.hpp>
#include <odr/internal/abstract/file.hpp>
#include <odr/internal/encoding/transcode.hpp>
#include <odr/internal/util/document_util.hpp>
#include <odr/internal/util/stream_util.hpp>

#include <algorithm>
#include <istream>
#include <memory>
#include <utility>

namespace odr::internal::csv {

namespace {

/// An id is the coordinate, not a registry index; `null_element_id` is zero,
/// so no kind may be. See `AGENTS.md`.
enum class Kind : std::uint64_t {
  root = 1,
  sheet = 2,
  cell = 3,
  text = 4,
};

constexpr std::uint64_t kind_shift = 61;
constexpr std::uint64_t row_shift = 24;
constexpr std::uint64_t column_mask = (std::uint64_t{1} << row_shift) - 1;
constexpr std::uint64_t row_mask =
    (std::uint64_t{1} << (kind_shift - row_shift)) - 1;

ElementIdentifier make_id(const Kind kind, const std::uint32_t column = 0,
                          const std::uint32_t row = 0) {
  return static_cast<std::uint64_t>(kind) << kind_shift |
         (static_cast<std::uint64_t>(row) & row_mask) << row_shift |
         (static_cast<std::uint64_t>(column) & column_mask);
}

Kind kind_of(const ElementIdentifier element_id) {
  return static_cast<Kind>(element_id >> kind_shift);
}

std::uint32_t row_of(const ElementIdentifier element_id) {
  return static_cast<std::uint32_t>(element_id >> row_shift & row_mask);
}

std::uint32_t column_of(const ElementIdentifier element_id) {
  return static_cast<std::uint32_t>(element_id & column_mask);
}

class ElementAdapter final : public abstract::ElementAdapter,
                             public abstract::SheetAdapter,
                             public abstract::SheetCellAdapter,
                             public abstract::TextAdapter {
public:
  explicit ElementAdapter(const CsvDocument &document)
      : m_document{&document} {}

  [[nodiscard]] ElementType
  element_type(const ElementIdentifier element_id) const override {
    switch (kind_of(element_id)) {
    case Kind::root:
      return ElementType::root;
    case Kind::sheet:
      return ElementType::sheet;
    case Kind::cell:
      return ElementType::sheet_cell;
    case Kind::text:
      return ElementType::text;
    default:
      return ElementType::none;
    }
  }

  [[nodiscard]] ElementIdentifier
  element_parent(const ElementIdentifier element_id) const override {
    switch (kind_of(element_id)) {
    case Kind::sheet:
      return make_id(Kind::root);
    case Kind::cell:
      return make_id(Kind::sheet);
    case Kind::text:
      return make_id(Kind::cell, column_of(element_id), row_of(element_id));
    default:
      return null_element_id;
    }
  }

  [[nodiscard]] ElementIdentifier
  element_first_child(const ElementIdentifier element_id) const override {
    switch (kind_of(element_id)) {
    case Kind::root:
      return make_id(Kind::sheet);
    case Kind::cell:
      return make_id(Kind::text, column_of(element_id), row_of(element_id));
    default:
      // a sheet's cells are reached by coordinate, not by walking
      return null_element_id;
    }
  }

  [[nodiscard]] ElementIdentifier
  element_last_child(const ElementIdentifier element_id) const override {
    return element_first_child(element_id);
  }

  [[nodiscard]] ElementIdentifier element_previous_sibling(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return null_element_id;
  }
  [[nodiscard]] ElementIdentifier element_next_sibling(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return null_element_id;
  }

  [[nodiscard]] bool element_is_unique(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return true;
  }
  [[nodiscard]] bool element_is_self_locatable(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return true;
  }
  [[nodiscard]] bool element_is_editable(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return false;
  }
  [[nodiscard]] DocumentPath
  element_document_path(const ElementIdentifier element_id) const override {
    return util::document::extract_path(*this, element_id, null_element_id);
  }
  [[nodiscard]] ElementIdentifier
  element_navigate_path(const ElementIdentifier element_id,
                        const DocumentPath &path) const override {
    return util::document::navigate_path(*this, element_id, path);
  }

  [[nodiscard]] const SheetAdapter *
  sheet_adapter(const ElementIdentifier element_id) const override {
    return kind_of(element_id) == Kind::sheet ? this : nullptr;
  }
  [[nodiscard]] const SheetCellAdapter *
  sheet_cell_adapter(const ElementIdentifier element_id) const override {
    return kind_of(element_id) == Kind::cell ? this : nullptr;
  }
  [[nodiscard]] const TextAdapter *
  text_adapter(const ElementIdentifier element_id) const override {
    return kind_of(element_id) == Kind::text ? this : nullptr;
  }

  // SheetAdapter

  [[nodiscard]] std::string sheet_name(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return "csv";
  }
  [[nodiscard]] TableDimensions sheet_dimensions(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return m_document->dimensions();
  }
  [[nodiscard]] TableDimensions
  sheet_content([[maybe_unused]] const ElementIdentifier element_id,
                const std::optional<TableDimensions> range) const override {
    const TableDimensions dimensions = m_document->dimensions();
    if (!range.has_value()) {
      return dimensions;
    }
    return {std::min(dimensions.rows, range->rows),
            std::min(dimensions.columns, range->columns)};
  }
  [[nodiscard]] ElementIdentifier
  sheet_cell([[maybe_unused]] const ElementIdentifier element_id,
             const std::uint32_t column,
             const std::uint32_t row) const override {
    return make_id(Kind::cell, column, row);
  }
  [[nodiscard]] ElementIdentifier sheet_first_shape(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return null_element_id;
  }
  [[nodiscard]] TableStyle sheet_style(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return {};
  }
  [[nodiscard]] TableColumnStyle sheet_column_style(
      [[maybe_unused]] const ElementIdentifier element_id,
      [[maybe_unused]] const std::uint32_t column) const override {
    return {};
  }
  [[nodiscard]] TableRowStyle
  sheet_row_style([[maybe_unused]] const ElementIdentifier element_id,
                  [[maybe_unused]] const std::uint32_t row) const override {
    return {};
  }
  [[nodiscard]] TableCellStyle
  sheet_cell_style([[maybe_unused]] const ElementIdentifier element_id,
                   [[maybe_unused]] const std::uint32_t column,
                   [[maybe_unused]] const std::uint32_t row) const override {
    return {};
  }

  // SheetCellAdapter

  [[nodiscard]] TablePosition
  sheet_cell_position(const ElementIdentifier element_id) const override {
    // `TablePosition` is (column, row); `TableDimensions` is (rows, columns)
    return TablePosition(column_of(element_id), row_of(element_id));
  }
  [[nodiscard]] bool sheet_cell_is_covered(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return false;
  }
  [[nodiscard]] TableDimensions sheet_cell_span(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return {1, 1};
  }
  [[nodiscard]] ValueType
  sheet_cell_value_type(const ElementIdentifier element_id) const override {
    return m_document->value_type(column_of(element_id), row_of(element_id));
  }

  // TextAdapter

  [[nodiscard]] std::string
  text_content(const ElementIdentifier element_id) const override {
    return std::string(
        m_document->cell(column_of(element_id), row_of(element_id)));
  }
  void
  text_set_content([[maybe_unused]] const ElementIdentifier element_id,
                   [[maybe_unused]] const std::string &text) const override {
    throw UnsupportedOperation();
  }
  [[nodiscard]] TextStyle text_style(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return {};
  }

private:
  const CsvDocument *m_document;
};

} // namespace

CsvDocument::CsvDocument(const abstract::File &file,
                         const TextEncoding encoding, const Dialect dialect,
                         const bool skip_first_line)
    : internal::Document(FileType::comma_separated_values,
                         DocumentType::spreadsheet, nullptr) {
  const std::unique_ptr<std::istream> in = file.stream();
  std::string text = encoding::to_utf8(util::stream::read(*in), encoding);

  std::string_view remainder = text;
  if (skip_first_line) {
    if (const std::size_t body = remainder.find_first_not_of(
            "\r\n", remainder.find_first_of("\r\n"));
        body != std::string_view::npos) {
      remainder = remainder.substr(body);
    } else {
      remainder = {};
    }
  }

  RecordReader reader(remainder, dialect);
  std::vector<std::string> fields;
  std::uint32_t columns = 0;
  while (reader.read(fields)) {
    columns = std::max(columns, static_cast<std::uint32_t>(fields.size()));
    m_rows.push_back(fields);
  }

  m_dimensions = {static_cast<std::uint32_t>(m_rows.size()), columns};

  // Walks the fields that exist rather than the rectangle they span: one wide
  // record widens every row, and scanning `rows * columns` synthesized cells
  // costs more than the file holds.
  //
  // The first row is a header, not a value — one word would otherwise make
  // every column prose.
  std::vector<bool> has_value(columns, false);
  m_numeric_columns.assign(columns, true);
  for (std::size_t row = 1; row < m_rows.size(); ++row) {
    const std::vector<std::string> &fields = m_rows[row];
    for (std::size_t column = 0; column < fields.size(); ++column) {
      const std::string &value = fields[column];
      if (value.empty()) {
        continue;
      }
      has_value[column] = true;
      if (!is_number(value)) {
        m_numeric_columns[column] = false;
      }
    }
  }
  for (std::uint32_t column = 0; column < columns; ++column) {
    m_numeric_columns[column] = m_numeric_columns[column] && has_value[column];
  }

  m_root_element = make_id(Kind::root);
  m_element_adapter = std::make_unique<ElementAdapter>(*this);
}

std::string_view CsvDocument::cell(const std::uint32_t column,
                                   const std::uint32_t row) const {
  if (row >= m_rows.size()) {
    return {};
  }
  // the sheet is rectangular even where the file is not
  const std::vector<std::string> &fields = m_rows[row];
  if (column >= fields.size()) {
    return {};
  }
  return fields[column];
}

TableDimensions CsvDocument::dimensions() const noexcept {
  return m_dimensions;
}

ValueType CsvDocument::value_type(const std::uint32_t column,
                                  const std::uint32_t row) const {
  if (column >= m_numeric_columns.size() || !m_numeric_columns[column]) {
    return ValueType::string;
  }
  // the header of a numeric column is still a name
  return is_number(cell(column, row)) ? ValueType::float_number
                                      : ValueType::string;
}

} // namespace odr::internal::csv
