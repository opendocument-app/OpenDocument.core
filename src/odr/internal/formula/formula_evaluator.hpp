#pragma once

#include <odr/sheet_position.hpp>
#include <odr/table_dimension.hpp>

#include <odr/internal/formula/formula_ast.hpp>
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
  sheet(std::string_view name) const = 0;
  /// What the cell at @p position holds: empty, a number, a string, a boolean
  /// or an error. Nothing where it has no answer, such as a formula cell whose
  /// result is stale.
  [[nodiscard]] virtual std::optional<Value>
  cell(const SheetPosition &position) const = 0;
  /// The columns and rows of @p sheet up to the last one stating content. A
  /// whole column (`A:A`) reaches to the last row of it.
  [[nodiscard]] virtual TableDimensions extent(std::uint32_t sheet) const = 0;
  /// Visits cells of @p area in reading order; null values mean no answer.
  /// Empty and out-of-extent cells may be skipped; the default visits every
  /// in-extent position.
  virtual void for_each_cell(
      const Area &area,
      const std::function<void(const SheetPosition &,
                               const std::optional<Value> &)> &visit) const;
  /// What @p name stands for in a formula on @p sheet: a name local to the
  /// sheet, else one of the whole document, read without case. Nothing where
  /// the document defines none, or one that does not parse.
  [[nodiscard]] virtual std::optional<Node>
  name([[maybe_unused]] std::string_view name,
       [[maybe_unused]] std::uint32_t sheet) const {
    return std::nullopt;
  }
};

/// Evaluates @p node at @p cell using @p settings, with implicit intersection
/// for range results. Returns null when the application-specific result
/// cannot be determined.
[[nodiscard]] std::optional<Value> evaluate(const Node &node,
                                            const SheetPosition &cell,
                                            const CellSource &source,
                                            const Settings &settings);

} // namespace odr::internal::formula
