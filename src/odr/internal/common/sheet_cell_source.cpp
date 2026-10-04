#include <odr/internal/common/sheet_cell_source.hpp>

#include <odr/internal/abstract/document.hpp>
#include <odr/internal/util/string_util.hpp>

#include <algorithm>
#include <map>
#include <utility>

namespace odr::internal {

namespace {

/// What the evaluator reads out of @p value. Nothing for a formula that
/// caches no result, and for an error whose spelling no formula states.
std::optional<formula::Value> value_of(const CellValue &value,
                                       const formula::Settings &settings) {
  using formula::Value;
  // LibreOffice before 4 and OpenOffice stated an error as the number 0
  // showing it: in the language of the user (`#VALORE!`), and as its own codes
  // (`Err:502`)
  if (value.has_formula() && value.type() == ValueType::float_number &&
      value.has_text()) {
    if (const std::optional<formula::ErrorType> error =
            formula::error_of_text(value.text())) {
      return Value{*error};
    }
    if (value.text().starts_with('#') || value.text().starts_with("Err:")) {
      return std::nullopt;
    }
  }
  switch (value.type()) {
  case ValueType::unknown:
    if (value.has_formula()) {
      return std::nullopt;
    }
    return Value{formula::Empty{}};
  case ValueType::string:
    return Value{value.has_text() ? value.text() : std::string()};
  case ValueType::float_number:
  case ValueType::time:
    if (!value.has_number()) {
      return std::nullopt;
    }
    return Value{value.number()};
  case ValueType::date:
    if (!value.has_number()) {
      return std::nullopt;
    }
    return Value{settings.serial(value.number())};
  case ValueType::boolean:
    if (!value.has_number()) {
      return std::nullopt;
    }
    return Value{value.number() != 0};
  case ValueType::error:
    if (const std::optional<formula::ErrorType> error =
            value.has_text() ? formula::error_of_text(value.text())
                             : std::nullopt) {
      return Value{*error};
    }
    return std::nullopt;
  }
  return std::nullopt;
}

/// Whether @p cell is the one at @p position. A merge covers the positions
/// of its span but the first, and an ods sheet answers the next cell of the
/// row for a covered position. Either reads as empty.
bool is_at(const SheetCell &cell, const TablePosition &position) {
  return !cell || cell.position() == position;
}

} // namespace

SheetCellSource::SheetCellSource(const abstract::Document &document)
    : m_settings{document.formula_settings()},
      m_syntax{formula::syntax_of(document.file_type())} {
  for (formula::Name &name : document.formula_names()) {
    m_names[util::string::to_lower(name.name)].push_back(std::move(name));
  }
  const abstract::ElementAdapter *adapter = document.element_adapter();
  m_adapter = adapter;
  if (adapter == nullptr) {
    return;
  }
  for (Element child = Element(adapter, document.root_element()).first_child();
       child; child = child.next_sibling()) {
    if (child.type() != ElementType::sheet) {
      continue;
    }
    const Sheet sheet = child.as_sheet();
    // a formula names a sheet without case, as the applications writing one do
    m_by_name.emplace(util::string::to_lower(sheet.name()),
                      static_cast<std::uint32_t>(m_sheets.size()));
    m_sheets.push_back(sheet);
  }
}

std::optional<std::uint32_t>
SheetCellSource::sheet(const std::string_view name) const {
  const auto found = m_by_name.find(util::string::to_lower(name));
  if (found == m_by_name.end()) {
    return std::nullopt;
  }
  return found->second;
}

std::optional<formula::Value>
SheetCellSource::cell(const SheetPosition &position) const {
  if (const auto known = m_cells.find(position); known != m_cells.end()) {
    return known->second;
  }
  std::optional<formula::Value> value;
  if (position.sheet < m_sheets.size()) {
    const SheetCell cell =
        m_sheets[position.sheet].cell(position.cell.column, position.cell.row);
    value = is_at(cell, position.cell) ? value_of(cell.value(), m_settings)
                                       : formula::Value{formula::Empty{}};
  }
  m_cells.emplace(position, value);
  return value;
}

TableDimensions SheetCellSource::extent(const std::uint32_t sheet) const {
  if (sheet >= m_sheets.size()) {
    return {};
  }
  if (const auto known = m_extents.find(sheet); known != m_extents.end()) {
    return known->second;
  }
  // the stated cells, a formula without a result too, which shows no content
  TableDimensions extent;
  if (const std::optional<std::vector<RowBand>> &sheet_bands = bands(sheet)) {
    for (const RowBand &band : *sheet_bands) {
      extent.rows = std::max(extent.rows, band.end_row);
      for (const CellRun &run : band.cells) {
        extent.columns = std::max(extent.columns, run.end_column);
      }
    }
  } else {
    extent = m_sheets[sheet].content(std::nullopt);
  }
  m_extents.emplace(sheet, extent);
  return extent;
}

std::optional<formula::Node>
SheetCellSource::name(const std::string_view name,
                      const std::uint32_t sheet) const {
  const auto found = m_names.find(util::string::to_lower(name));
  if (found == m_names.end() || !m_syntax.has_value()) {
    return std::nullopt;
  }
  const formula::Name *global = nullptr;
  for (const formula::Name &candidate : found->second) {
    if (candidate.sheet == sheet) {
      return formula::parse(candidate.expression, *m_syntax);
    }
    if (!candidate.sheet.has_value() && global == nullptr) {
      global = &candidate;
    }
  }
  if (global == nullptr) {
    return std::nullopt;
  }
  return formula::parse(global->expression, *m_syntax);
}

const std::optional<std::vector<SheetCellSource::RowBand>> &
SheetCellSource::bands(const std::uint32_t sheet) const {
  if (const auto known = m_bands.find(sheet); known != m_bands.end()) {
    return known->second;
  }
  std::optional<std::vector<RowBand>> result;
  const ElementIdentifier sheet_id = m_sheets[sheet].identifier();
  std::map<std::pair<std::uint32_t, std::uint32_t>, std::vector<CellRun>>
      by_rows;
  if (m_adapter->sheet_adapter(sheet_id)->sheet_visit_cells(
          sheet_id, [&](const std::uint32_t column, const std::uint32_t row,
                        const TableDimensions &repeated,
                        const ElementIdentifier cell_id) {
            by_rows[{row, row + repeated.rows}].push_back(CellRun{
                column, column + repeated.columns,
                value_of(Element(m_adapter, cell_id).as_sheet_cell().value(),
                         m_settings)});
          })) {
    result.emplace();
    for (auto &[rows, cells] : by_rows) {
      std::ranges::sort(cells, {}, &CellRun::first_column);
      result->push_back(RowBand{rows.first, rows.second, std::move(cells)});
    }
  }
  return m_bands.emplace(sheet, std::move(result)).first->second;
}

void SheetCellSource::for_each_cell(
    const formula::Area &area,
    const std::function<void(const SheetPosition &,
                             const std::optional<formula::Value> &)> &visit)
    const {
  if (area.sheet >= m_sheets.size()) {
    return;
  }
  const std::optional<std::vector<RowBand>> &sheet_bands = bands(area.sheet);
  if (!sheet_bands.has_value()) {
    CellSource::for_each_cell(area, visit);
    return;
  }
  const TablePosition &from = area.range.from();
  const TablePosition &to = area.range.to();
  for (const RowBand &band : *sheet_bands) {
    if (band.end_row <= from.row || band.first_row > to.row) {
      continue;
    }
    for (std::uint32_t row = std::max(band.first_row, from.row);
         row < band.end_row && row <= to.row; ++row) {
      for (const CellRun &run : band.cells) {
        for (std::uint32_t column = std::max(run.first_column, from.column);
             column < run.end_column && column <= to.column; ++column) {
          visit(SheetPosition(area.sheet, column, row), run.value);
        }
      }
    }
  }
}

const formula::Settings &SheetCellSource::settings() const noexcept {
  return m_settings;
}

const std::vector<Sheet> &SheetCellSource::sheets() const noexcept {
  return m_sheets;
}

} // namespace odr::internal
