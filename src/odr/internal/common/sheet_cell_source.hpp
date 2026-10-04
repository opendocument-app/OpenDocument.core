#pragma once

#include <odr/document_element.hpp>
#include <odr/sheet_position.hpp>
#include <odr/table_dimension.hpp>

#include <odr/internal/formula/formula_evaluator.hpp>
#include <odr/internal/formula/formula_parser.hpp>
#include <odr/internal/formula/formula_value.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace odr::internal::abstract {
class Document;
} // namespace odr::internal::abstract

namespace odr::internal {

/// The cells of a decoded document, as the evaluator reads them. A formula
/// cell gives the result the file caches, and no answer where it caches none.
class SheetCellSource final : public formula::CellSource {
public:
  explicit SheetCellSource(const abstract::Document &document);

  [[nodiscard]] std::optional<std::uint32_t>
  sheet(std::string_view name) const override;
  [[nodiscard]] std::optional<formula::Value>
  cell(const SheetPosition &position) const override;
  [[nodiscard]] TableDimensions extent(std::uint32_t sheet) const override;
  void
  for_each_cell(const formula::Area &area,
                const std::function<void(const SheetPosition &,
                                         const std::optional<formula::Value> &)>
                    &visit) const override;
  [[nodiscard]] std::optional<formula::Node>
  name(std::string_view name, std::uint32_t sheet) const override;

  /// The settings the document states, which a cell's date is read with.
  [[nodiscard]] const formula::Settings &settings() const noexcept;
  /// The sheets of the document, in order.
  [[nodiscard]] const std::vector<Sheet> &sheets() const noexcept;

private:
  /// A run of cells one row band states alike: the columns, and what each
  /// cell holds.
  struct CellRun final {
    std::uint32_t first_column{0};
    std::uint32_t end_column{0};
    std::optional<formula::Value> value{};
  };
  /// Rows that state their cells alike: a repeated row, or a single one.
  struct RowBand final {
    std::uint32_t first_row{0};
    std::uint32_t end_row{0};
    std::vector<CellRun> cells{};
  };

  /// The bands of @p sheet in reading order, nothing where its engine visits
  /// no cells.
  [[nodiscard]] const std::optional<std::vector<RowBand>> &
  bands(std::uint32_t sheet) const;

  const abstract::ElementAdapter *m_adapter{nullptr};
  std::vector<Sheet> m_sheets;
  /// The sheets by their name in lower case.
  std::unordered_map<std::string, std::uint32_t> m_by_name;
  formula::Settings m_settings;
  std::optional<formula::Syntax> m_syntax;
  /// The names of the document by their name in lower case.
  std::unordered_map<std::string, std::vector<formula::Name>> m_names;
  mutable std::unordered_map<SheetPosition, std::optional<formula::Value>>
      m_cells;
  mutable std::unordered_map<std::uint32_t, TableDimensions> m_extents;
  mutable std::unordered_map<std::uint32_t, std::optional<std::vector<RowBand>>>
      m_bands;
};

} // namespace odr::internal
