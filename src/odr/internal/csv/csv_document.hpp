#pragma once

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

  [[nodiscard]] bool is_editable() const noexcept override;
  [[nodiscard]] bool is_savable(bool encrypted) const noexcept override;

  void save(const Path &path) const override;
  void save(const Path &path, const char *password) const override;

  /// The cell's text, empty where a row stops short.
  [[nodiscard]] std::string_view cell(std::uint32_t column,
                                      std::uint32_t row) const;
  [[nodiscard]] TableDimensions dimensions() const noexcept;

private:
  std::vector<std::vector<std::string>> m_rows;
  TableDimensions m_dimensions;
};

} // namespace odr::internal::csv
