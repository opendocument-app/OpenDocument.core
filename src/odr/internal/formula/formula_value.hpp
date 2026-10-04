#pragma once

#include <odr/internal/common/table_range.hpp>
#include <odr/internal/formula/formula_ast.hpp>
#include <odr/internal/number_format/number_format.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace odr::internal::formula {

/// Whose rules the evaluator follows where the two applications differ.
enum class Dialect {
  libreoffice, ///< a boolean is the number 1 or 0, and `0^0` is 1
  excel,       ///< a boolean is a type of its own, and `0^0` is `#NUM!`
};

/// How a document's formulas compute. An `.ods` states it in
/// `table:calculation-settings`, and an `.xlsx` takes Excel's, as
/// LibreOffice does on import.
struct Settings final {
  Dialect dialect{Dialect::excel};
  bool case_sensitive{false};
  bool wildcards{true};
  bool regular_expressions{false};
  /// Whether a criterion has to match the whole cell, not a part of it.
  bool whole_cell{true};
  /// What an `.xlsx` counts a date serial from.
  number_format::Epoch epoch{number_format::Epoch::from_1900};
  /// The days from 1899-12-30 to the `table:null-date` of an `.ods`.
  std::int64_t null_date{0};
  /// The first year a two-digit year of an `.ods` stands for: with 1930,
  /// `30` is 1930 and `29` is 2029.
  std::int64_t null_year{1930};

  /// The serial a formula computes with for @p days since 1899-12-30.
  [[nodiscard]] double serial(double days) const;
  /// The days since 1899-12-30 a formula's @p serial stands for.
  [[nodiscard]] double days(double serial) const;
};

/// A cell that states nothing.
struct Empty final {
  friend bool operator==(const Empty &, const Empty &) = default;
};

/// A rectangle of one sheet.
struct Area final {
  std::uint32_t sheet{0};
  /// A whole column or row reaches only to the extent of the sheet here.
  TableRange range{};
  /// Whether the reference states no row (`A:A`) or no column (`1:1`), so
  /// the rectangle is as tall or as wide as the grid of the application.
  bool whole_columns{false};
  bool whole_rows{false};

  friend bool operator==(const Area &, const Area &) = default;
};

/// What a reference evaluates to before an operator or a function reads its
/// cells: one area, or several after `~`.
struct Reference final {
  std::vector<Area> areas;

  friend bool operator==(const Reference &, const Reference &) = default;
};

struct Value;

/// An array literal, or what an operator makes of an array. The cells are
/// scalars, row by row.
struct Matrix final {
  std::uint32_t columns{0};
  std::uint32_t rows{0};
  std::vector<Value> cells;

  friend bool operator==(const Matrix &, const Matrix &);
};

/// What a formula, or a part of it, evaluates to. A date and a time are the
/// serial a formula computes with.
struct Value final {
  using Content = std::variant<Empty, double, std::string, bool, ErrorType,
                               Reference, Matrix>;

  Content content{};

  template <typename T> [[nodiscard]] bool holds() const noexcept {
    return std::holds_alternative<T>(content);
  }
  /// @throws std::bad_variant_access where the value holds something else.
  template <typename T> [[nodiscard]] const T &get() const {
    return std::get<T>(content);
  }

  friend bool operator==(const Value &, const Value &) = default;
};

inline bool operator==(const Matrix &a, const Matrix &b) {
  return a.columns == b.columns && a.rows == b.rows && a.cells == b.cells;
}

/// A number, or the error that stands in for one.
using Number = std::variant<double, ErrorType>;
/// A text, or the error that stands in for one.
using Text = std::variant<std::string, ErrorType>;

/// Whether @p a and @p b are one number, as a sheet compares two: equal where
/// they agree to about 15 significant digits.
[[nodiscard]] bool approximately_equal(double a, double b);

/// `a + b`, and 0 where the two cancel to within the precision of a sheet,
/// as LibreOffice's `rtl::math::approxAdd` does: `0.1+0.2-0.3` is 0.
[[nodiscard]] double approximate_add(double a, double b);

/// Whether @p sum is so near 0 against @p largest, the largest magnitude it
/// adds, that Excel may make it 0. Excel does not document how near.
[[nodiscard]] bool nearly_cancels(double sum, double largest);

/// The number a text reads as in arithmetic. `#VALUE!` where the text holds no
/// digit, and nothing where it holds one in a form whose reading depends on
/// the locale: a currency, a date, a grouped number.
[[nodiscard]] std::optional<Number> number_of_text(std::string_view text);

/// The text a number reads as in a concatenation: up to 15 significant
/// digits. Nothing where the two applications spell it apart, past 15
/// integer digits and below 0.0001.
[[nodiscard]] std::optional<std::string> text_of_number(double number);

/// Every error a formula spells.
inline constexpr std::array<ErrorType, 7> error_types{
    ErrorType::null,         ErrorType::division, ErrorType::value,
    ErrorType::reference,    ErrorType::name,     ErrorType::number,
    ErrorType::not_available};

/// The spelling of @p error in a cell: `#DIV/0!`.
[[nodiscard]] std::string_view to_string(ErrorType error);
/// The error a cell's text spells, nothing where it spells none.
[[nodiscard]] std::optional<ErrorType> error_of_text(std::string_view text);

} // namespace odr::internal::formula
