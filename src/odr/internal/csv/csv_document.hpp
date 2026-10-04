#pragma once

#include <odr/document_element.hpp>
#include <odr/file.hpp>
#include <odr/table_dimension.hpp>

#include <odr/internal/common/document.hpp>
#include <odr/internal/csv/csv_util.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace odr::internal::abstract {
class File;
}

namespace odr::internal::csv {

/// A csv as a one-sheet spreadsheet.
///
/// Cells are not registry elements; an id encodes the coordinate. The whole
/// file is held decoded and reached only through @ref cell and @ref dimensions,
/// so what is behind those can change. See `AGENTS.md`.
class CsvDocument final : public internal::Document {
public:
  CsvDocument(const abstract::File &file, TextEncoding encoding,
              Dialect dialect, bool skip_first_line);

  /// The cell's text, empty where a row stops short.
  [[nodiscard]] std::string_view cell(std::uint32_t column,
                                      std::uint32_t row) const;
  [[nodiscard]] TableDimensions dimensions() const noexcept;

  /// Only @ref ValueType::float_number in a column whose values are all
  /// numbers — a lone number in a column of prose is not a quantity.
  [[nodiscard]] ValueType value_type(std::uint32_t column,
                                     std::uint32_t row) const;

  /// Writes @p text at a position, growing the sheet to reach it.
  void set_cell(std::uint32_t column, std::uint32_t row, std::string text);

  [[nodiscard]] bool is_editable() const noexcept override;
  [[nodiscard]] bool is_savable(bool encrypted) const noexcept override;
  /// UTF-8, whatever the source encoding was, with a byte order mark unless
  /// the source was UTF-8 without one. A field is quoted only where it has to
  /// be.
  void save(std::ostream &out) const override;

private:
  Dialect m_dialect;
  bool m_separator_directive{false};
  bool m_byte_order_mark{false};
  std::string m_line_end;
  bool m_final_line_end{true};

  std::vector<std::vector<std::string>> m_rows;
  TableDimensions m_dimensions;
  /// Per column, whether every value below the first row is a number.
  std::vector<bool> m_numeric_columns;

  void type_column(std::uint32_t column);
};

} // namespace odr::internal::csv
