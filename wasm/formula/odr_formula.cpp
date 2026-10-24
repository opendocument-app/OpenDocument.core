#include <odr/internal/formula/formula_dependencies.hpp>
#include <odr/internal/formula/formula_evaluator.hpp>
#include <odr/internal/formula/formula_parser.hpp>
#include <odr/internal/formula/formula_value.hpp>
#include <odr/internal/formula/formula_writer.hpp>
#include <odr/internal/number_format/number_format.hpp>
#include <odr/internal/util/string_util.hpp>
#include <odr/sheet_position.hpp>
#include <odr/table_dimension.hpp>

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

using emscripten::val;
using namespace odr;
using namespace odr::internal;

bool is_absent(const val &value) {
  return value.isNull() || value.isUndefined();
}

std::optional<formula::Syntax> syntax_of(const std::string &name) {
  if (name == "opendocument") {
    return formula::Syntax::opendocument;
  }
  if (name == "ooxml") {
    return formula::Syntax::ooxml;
  }
  return std::nullopt;
}

template <typename T> void read(const val &object, const char *key, T &target) {
  if (const val value = object[key]; !is_absent(value)) {
    target = value.as<T>();
  }
}

/// A plain object with the keys of `formula::Settings`; an absent key keeps
/// the default.
formula::Settings settings_of(const val &object) {
  formula::Settings result;
  if (is_absent(object)) {
    return result;
  }
  if (const val dialect = object["dialect"]; !is_absent(dialect)) {
    result.dialect = dialect.as<std::string>() == "libreoffice"
                         ? formula::Dialect::libreoffice
                         : formula::Dialect::excel;
  }
  read(object, "caseSensitive", result.case_sensitive);
  read(object, "wildcards", result.wildcards);
  read(object, "regularExpressions", result.regular_expressions);
  read(object, "wholeCell", result.whole_cell);
  if (const val epoch = object["epoch"]; !is_absent(epoch)) {
    result.epoch = epoch.as<std::int32_t>() == 1904
                       ? number_format::Epoch::from_1904
                       : number_format::Epoch::from_1900;
  }
  // a JavaScript number, which embind reads as an `std::int64_t` only from a
  // BigInt
  double null_date = static_cast<double>(result.null_date);
  double null_year = static_cast<double>(result.null_year);
  read(object, "nullDate", null_date);
  read(object, "nullYear", null_year);
  result.null_date = static_cast<std::int64_t>(null_date);
  result.null_year = static_cast<std::int64_t>(null_year);
  if (const val word = object["booleanWord"]; !is_absent(word)) {
    result.boolean_word = word.as<bool>();
  }
  return result;
}

/// `{type, number | text | boolean}` as a cell holds it, nothing for no answer.
std::optional<formula::Value> value_of(const val &object) {
  if (is_absent(object)) {
    return std::nullopt;
  }
  const std::string type = object["type"].as<std::string>();
  if (type == "empty") {
    return formula::Value{formula::Empty{}};
  }
  if (type == "number") {
    return formula::Value{object["number"].as<double>()};
  }
  if (type == "string") {
    return formula::Value{object["text"].as<std::string>()};
  }
  if (type == "boolean") {
    return formula::Value{object["boolean"].as<bool>()};
  }
  if (type == "error") {
    if (const std::optional<formula::ErrorType> error =
            formula::error_of_text(object["text"].as<std::string>());
        error.has_value()) {
      return formula::Value{*error};
    }
  }
  return std::nullopt;
}

val to_js(const formula::Value &value) {
  val result = val::object();
  if (value.holds<formula::Empty>()) {
    result.set("type", "empty");
  } else if (value.holds<double>()) {
    result.set("type", "number");
    result.set("number", value.get<double>());
  } else if (value.holds<std::string>()) {
    result.set("type", "string");
    result.set("text", value.get<std::string>());
  } else if (value.holds<bool>()) {
    result.set("type", "boolean");
    result.set("boolean", value.get<bool>());
  } else if (value.holds<formula::ErrorType>()) {
    result.set("type", "error");
    result.set("text", std::string(formula::to_string(
                           value.get<formula::ErrorType>())));
  } else {
    return val::null();
  }
  return result;
}

/// The cells, in the callbacks of a JavaScript object. A callback must not
/// throw: a JavaScript exception unwinds through the evaluator.
class JsCellSource final : public formula::CellSource {
public:
  JsCellSource(val source, const formula::Syntax syntax)
      : m_source{std::move(source)}, m_syntax{syntax} {}

  [[nodiscard]] std::optional<std::uint32_t>
  sheet(const std::string_view name) const override {
    const val result = m_source.call<val>("sheet", std::string(name));
    if (is_absent(result)) {
      return std::nullopt;
    }
    return result.as<std::uint32_t>();
  }

  [[nodiscard]] std::optional<formula::Value>
  cell(const SheetPosition &position) const override {
    return value_of(m_source.call<val>(
        "cell", position.sheet, position.cell.column, position.cell.row));
  }

  [[nodiscard]] TableDimensions
  extent(const std::uint32_t sheet) const override {
    const val result = m_source.call<val>("extent", sheet);
    return TableDimensions(result["rows"].as<std::uint32_t>(),
                           result["columns"].as<std::uint32_t>());
  }

  [[nodiscard]] std::optional<formula::Node>
  name(const std::string_view name, const std::uint32_t sheet) const override {
    if (is_absent(m_source["name"])) {
      return std::nullopt;
    }
    const val expression = m_source.call<val>("name", std::string(name), sheet);
    if (is_absent(expression)) {
      return std::nullopt;
    }
    return formula::parse(expression.as<std::string>(), m_syntax);
  }

private:
  val m_source;
  formula::Syntax m_syntax;
};

/// What @p expression computes at a cell, null where the evaluator has no
/// answer, as `formula::evaluate` gives none.
val evaluate(const std::string &expression, const std::string &syntax,
             const std::uint32_t sheet, const std::uint32_t column,
             const std::uint32_t row, const val &settings, const val &source) {
  const std::optional<formula::Syntax> parsed_syntax = syntax_of(syntax);
  if (!parsed_syntax.has_value()) {
    return val::null();
  }
  const std::optional<formula::Node> node =
      formula::parse(expression, *parsed_syntax);
  if (!node.has_value()) {
    return val::null();
  }
  const JsCellSource cells(source, *parsed_syntax);
  const std::optional<formula::Value> result = formula::evaluate(
      *node, SheetPosition(sheet, column, row), cells, settings_of(settings));
  return result.has_value() ? to_js(*result) : val::null();
}

/// The text a cell with the format @p code shows for @p number, in the
/// language of the BCP 47 tag @p locale. Null where @p code is no format code.
val format(const double number, const std::string &code,
           const std::string &locale, const bool from_1904) {
  try {
    const number_format::Format parsed =
        code.empty() ? number_format::Format() : number_format::Format(code);
    return val(parsed.format(number,
                             from_1904 ? number_format::Epoch::from_1904
                                       : number_format::Epoch::from_1900,
                             number_format::symbols_of(locale)));
  } catch (const std::invalid_argument &) {
    return val::null();
  }
}

/// What the first section of @p code shows a number as: `number`, `date` or
/// `time`. Null where @p code is no format code.
val category(const std::string &code) {
  try {
    switch (code.empty() ? number_format::Category::number
                         : number_format::Format(code).category()) {
    case number_format::Category::date:
      return val("date");
    case number_format::Category::time:
      return val("time");
    case number_format::Category::number:
      return val("number");
    }
  } catch (const std::invalid_argument &) {
  }
  return val::null();
}

/// The serial a formula computes with for @p days since 1899-12-30.
double serial(const double days, const val &settings) {
  return settings_of(settings).serial(days);
}

/// The days since 1899-12-30 a formula's @p serial stands for.
double days(const double serial, const val &settings) {
  return settings_of(settings).days(serial);
}

/// @p node as a cell of @p syntax states it: `of:=` ahead of OpenFormula.
std::string spelled(const formula::Node &node, const formula::Syntax syntax) {
  return (syntax == formula::Syntax::opendocument ? "of:=" : "") +
         formula::to_string(node, syntax);
}

/// What the user typed after the `=`, as an `<f>` spells it, in the syntax of
/// the file. Null where it does not parse.
val to_file(const std::string &input, const std::string &syntax) {
  const std::optional<formula::Syntax> file_syntax = syntax_of(syntax);
  if (!file_syntax.has_value()) {
    return val::null();
  }
  const std::optional<formula::Node> node =
      formula::parse(input, formula::Syntax::ooxml);
  return node.has_value() ? val(spelled(*node, *file_syntax)) : val::null();
}

/// A formula of the file as the user types it, without the `=`: the reverse
/// of `to_file`. Null where it does not parse.
val to_input(const std::string &expression, const std::string &syntax) {
  const std::optional<formula::Syntax> file_syntax = syntax_of(syntax);
  if (!file_syntax.has_value()) {
    return val::null();
  }
  const std::optional<formula::Node> node =
      formula::parse(expression, *file_syntax);
  return node.has_value()
             ? val(formula::to_string(*node, formula::Syntax::ooxml))
             : val::null();
}

/// What @p expression reads, as `data-odr-reads` states it. A sheet other
/// than the one named @p sheet_name reads all of @p sheet.
val reads(const std::string &expression, const std::string &syntax,
          const std::uint32_t sheet, const std::string &sheet_name) {
  const std::optional<formula::Syntax> file_syntax = syntax_of(syntax);
  if (!file_syntax.has_value()) {
    return val::null();
  }
  const std::optional<formula::Node> node =
      formula::parse(expression, *file_syntax);
  if (!node.has_value()) {
    return val::null();
  }
  const std::string own = util::string::to_lower(sheet_name);
  return val(formula::spell_reads(
      *node, sheet,
      [&](const std::string_view name) -> std::optional<std::uint32_t> {
        return util::string::to_lower(name) == own
                   ? std::optional<std::uint32_t>(sheet)
                   : std::nullopt;
      }));
}

/// `[true, false]`: the words a boolean result shows as in a document of
/// @p locale, whose cells show @p shown_true and @p shown_false, empty where
/// none does.
val boolean_words(const std::string &locale, const std::string &shown_true,
                  const std::string &shown_false) {
  const number_format::BooleanWords words =
      number_format::boolean_words(locale, shown_true, shown_false);
  val result = val::array();
  result.call<void>("push", words.true_word);
  result.call<void>("push", words.false_word);
  return result;
}

} // namespace

EMSCRIPTEN_BINDINGS(odr_formula) {
  emscripten::function("evaluate", &evaluate);
  emscripten::function("format", &format);
  emscripten::function("category", &category);
  emscripten::function("serial", &serial);
  emscripten::function("days", &days);
  emscripten::function("toFile", &to_file);
  emscripten::function("toInput", &to_input);
  emscripten::function("reads", &reads);
  emscripten::function("booleanWords", &boolean_words);
}
