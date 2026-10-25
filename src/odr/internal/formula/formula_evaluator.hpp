#pragma once

#include <odr/sheet_position.hpp>
#include <odr/table_dimension.hpp>

#include <odr/internal/formula/formula_ast.hpp>
#include <odr/internal/formula/formula_result.hpp>
#include <odr/internal/formula/formula_value.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

namespace odr::internal::formula {

/// Where the evaluator reads the cells a formula names.
class CellSource {
public:
  virtual ~CellSource() = default;

  /// The sheet a formula names as @p name, which is read without case.
  /// Nothing where the document has no such sheet.
  [[nodiscard]] virtual std::optional<std::uint32_t>
  sheet(std::string_view name) const noexcept = 0;
  /// What the cell at @p position holds: empty, a number, a string, a boolean
  /// or an error. Nothing where it has no answer, such as a formula cell whose
  /// result is stale.
  [[nodiscard]] virtual std::optional<Value>
  cell(const SheetPosition &position) const noexcept = 0;
  /// The columns and rows of @p sheet up to the last one stating content. A
  /// whole column (`A:A`) reaches to the last row of it.
  [[nodiscard]] virtual TableDimensions
  extent(std::uint32_t sheet) const noexcept = 0;
  /// Visits cells of @p area in reading order; null values mean no answer.
  /// Empty and out-of-extent cells may be skipped; the default visits every
  /// in-extent position. A stop of @p visit ends the walk and is its result.
  [[nodiscard]] virtual Result<void>
  for_each_cell(const Area &area,
                const std::function<Result<void>(const SheetPosition &,
                                                 const std::optional<Value> &)>
                    &visit) const noexcept;
  /// The sheet that holds the cached copy of @p sheet of another document,
  /// which a formula names as @p document: `1` in `[1]Sheet1!A1`, the url
  /// in `['file:///x.ods'#$Sheet1.A1]`. Nothing where the file caches none.
  [[nodiscard]] virtual std::optional<std::uint32_t>
  external_sheet([[maybe_unused]] std::string_view document,
                 [[maybe_unused]] std::string_view sheet) const noexcept {
    return std::nullopt;
  }
  /// What @p name stands for in a formula on @p sheet: a name local to the
  /// sheet, else one of the whole document, read without case. Nothing where
  /// the document defines none, or one that does not parse.
  [[nodiscard]] virtual std::optional<Node>
  name([[maybe_unused]] std::string_view name,
       [[maybe_unused]] std::uint32_t sheet) const noexcept {
    return std::nullopt;
  }
};

/// Evaluates @p node at @p cell using @p settings, with implicit intersection
/// for range results. Returns null when the application-specific result
/// cannot be determined.
[[nodiscard]] std::optional<Value> evaluate(const Node &node,
                                            const SheetPosition &cell,
                                            const CellSource &source,
                                            const Settings &settings) noexcept;

/// Evaluates @p node as an array formula anchored at @p cell: an operator
/// reads every cell of a range, and a function maps over the elements of an
/// argument it takes as one value. Null where it has no answer.
[[nodiscard]] std::optional<Matrix>
evaluate_array(const Node &node, const SheetPosition &cell,
               const CellSource &source, const Settings &settings) noexcept;

/// What an array formula's @p result puts at (@p column, @p row) of the
/// range it fills: a single row or column repeats, and a position past the
/// result is `#N/A`.
[[nodiscard]] Value spread(const Matrix &result, std::uint32_t column,
                           std::uint32_t row) noexcept;

} // namespace odr::internal::formula
