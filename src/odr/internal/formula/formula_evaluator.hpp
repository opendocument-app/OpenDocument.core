#pragma once

#include <odr/sheet_position.hpp>
#include <odr/table_dimension.hpp>

#include <odr/internal/formula/formula_ast.hpp>
#include <odr/internal/formula/formula_value.hpp>

#include <cstdint>
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
  /// What @p name stands for in a formula on @p sheet: a name local to the
  /// sheet, else one of the whole document, read without case. Nothing where
  /// the document defines none, or one that does not parse.
  [[nodiscard]] virtual std::optional<Node>
  name([[maybe_unused]] std::string_view name,
       [[maybe_unused]] std::uint32_t sheet) const {
    return std::nullopt;
  }
};

/// The value of @p node, the formula of the cell at @p cell. A result that
/// is a range reads the cell the formula's row or column crosses, as a cell
/// without an array formula does.
///
/// Nothing where the evaluator has no answer: for a function it does not
/// know, a name, a reference into another document, a cell with no answer,
/// or a text whose number depends on the locale. An answer it gives is the
/// one LibreOffice or Excel gives, as @p settings says.
[[nodiscard]] std::optional<Value> evaluate(const Node &node,
                                            const SheetPosition &cell,
                                            const CellSource &source,
                                            const Settings &settings);

} // namespace odr::internal::formula
