#pragma once

#include <odr/internal/formula/formula_evaluator.hpp>
#include <odr/internal/formula/formula_parser.hpp>
#include <odr/internal/formula/formula_value.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>

namespace odr::test::formula {

using namespace odr::internal::formula;

/// Two sheets, `s` and `t`, of the cells a test states. A cell it does not
/// state is empty, and a stale one has no answer.
class Cells final : public CellSource {
public:
  std::map<SheetPosition, Value> values;
  std::set<SheetPosition> stale;

  [[nodiscard]] std::optional<std::uint32_t>
  sheet(const std::string_view name) const override {
    if (name == "s" || name == "S") {
      return 0;
    }
    if (name == "t" || name == "T") {
      return 1;
    }
    return std::nullopt;
  }
  [[nodiscard]] std::optional<Value>
  cell(const SheetPosition &position) const override {
    if (stale.contains(position)) {
      return std::nullopt;
    }
    const auto found = values.find(position);
    return found == values.end() ? Value{Empty{}} : found->second;
  }
  [[nodiscard]] TableDimensions
  extent(const std::uint32_t sheet) const override {
    TableDimensions result;
    for (const auto &[position, value] : values) {
      if (position.sheet == sheet) {
        result.rows = std::max(result.rows, position.cell.row + 1);
        result.columns = std::max(result.columns, position.cell.column + 1);
      }
    }
    return result;
  }
};

/// Column A of `s`: the texts `1`, `abc`, nothing, the number 2, true, and
/// an empty text. A30 is 7, and row 30 is the row of the formula.
inline Cells cells() {
  Cells result;
  result.values[SheetPosition(0, 0, 0)] = Value{std::string("1")};
  result.values[SheetPosition(0, 0, 1)] = Value{std::string("abc")};
  result.values[SheetPosition(0, 0, 3)] = Value{2.0};
  result.values[SheetPosition(0, 0, 4)] = Value{true};
  result.values[SheetPosition(0, 0, 5)] = Value{std::string()};
  result.values[SheetPosition(0, 0, 29)] = Value{7.0};
  result.values[SheetPosition(1, 0, 0)] = Value{5.0};
  return result;
}

inline const SheetPosition formula_cell(0, 5, 29);

inline std::optional<Value> evaluated(const std::string &formula,
                                      const Syntax syntax,
                                      const Settings &settings,
                                      const Cells &source = cells()) {
  const std::optional<Node> node = parse(formula, syntax);
  if (!node.has_value()) {
    ADD_FAILURE() << "does not parse: " << formula;
    return std::nullopt;
  }
  return evaluate(*node, formula_cell, source, settings);
}

/// As LibreOffice computes a formula of an `.ods` that states no settings.
inline std::optional<Value> ods(const std::string &formula,
                                const Cells &source = cells()) {
  return evaluated(formula, Syntax::opendocument,
                   Settings{.dialect = Dialect::libreoffice,
                            .case_sensitive = true,
                            .wildcards = false,
                            .regular_expressions = true},
                   source);
}

/// As Excel computes a formula of an `.xlsx`.
inline std::optional<Value> xlsx(const std::string &formula,
                                 const Cells &source = cells()) {
  return evaluated(formula, Syntax::ooxml, Settings{}, source);
}

inline Value number(const double value) { return Value{value}; }
inline Value text(const std::string &value) { return Value{value}; }
inline Value boolean(const bool value) { return Value{value}; }
inline Value error(const ErrorType value) { return Value{value}; }

} // namespace odr::test::formula
