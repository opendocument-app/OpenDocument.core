#include <odr/internal/formula/formula_function.hpp>

#include <odr/internal/util/string_util.hpp>

#include <array>
#include <cmath>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <variant>

namespace odr::internal::formula {

namespace {

/// A truth value, or the error standing for one.
using Truth = std::variant<bool, ErrorType>;

/// A text stated as a truth value. LibreOffice refuses every text, and Excel
/// reads `TRUE` and `FALSE`, as its documentation does not say how.
Result<ErrorType> truth_of_text(const std::string &text) noexcept {
  if (util::string::equals_ignore_case(text, "TRUE") ||
      util::string::equals_ignore_case(text, "FALSE")) {
    return no_answer();
  }
  return ErrorType::value;
}

/// @p value read as a condition: an empty cell is false, a number true
/// unless it is 0.
Result<Truth> truth(const Value &value) noexcept {
  if (const auto *boolean = std::get_if<bool>(&value.content)) {
    return *boolean;
  }
  if (const auto *number = std::get_if<double>(&value.content)) {
    return *number != 0;
  }
  if (value.holds<Empty>()) {
    return false;
  }
  if (const auto *error = std::get_if<ErrorType>(&value.content)) {
    return *error;
  }
  const auto *text = std::get_if<std::string>(&value.content);
  if (text == nullptr) {
    return no_answer();
  }
  return truth_of_text(*text).transform(
      [](const ErrorType error) { return Truth{error}; });
}

Result<Value> condition(const Call &call) {
  if (call.size() < 1 || call.size() > 3 || call.missing(0)) {
    return no_answer();
  }
  const Result<Truth> test = call.scalar(0).and_then(truth);
  if (!test.has_value()) {
    return std::unexpected(test.error());
  }
  if (const auto *error = std::get_if<ErrorType>(&*test)) {
    return Value{*error};
  }
  const bool passed = *std::get_if<bool>(&*test);
  const std::size_t branch = passed ? 1 : 2;
  if (branch >= call.size()) {
    // `IF(c)` and `IF(c; x)` give the condition where no branch is stated
    return Value{passed};
  }
  if (call.missing(branch)) {
    return Value{0.0};
  }
  return call.value(branch);
}

/// `IFERROR` and `IFNA`: the second argument where the first is an error
/// @p catches answers to.
template <bool only_not_available> Result<Value> if_error(const Call &call) {
  if (const Result<void> expected = expect_arguments(call, 2, 2);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  Result<Value> value = call.scalar(0);
  if (!value.has_value()) {
    return value;
  }
  const auto *error = std::get_if<ErrorType>(&value->content);
  if (error != nullptr &&
      (!only_not_available || *error == ErrorType::not_available)) {
    return call.value(1);
  }
  return value;
}

/// `AND`, `OR` and `XOR` over the truth values of their arguments. A range
/// adds its booleans and numbers, a text and an empty cell nothing. With
/// none at all the result is `#VALUE!`.
template <typename Combine>
Result<Value> combine(const Call &call, const bool start, const Combine &next) {
  if (const Result<void> expected = expect_arguments(call, 1, 255);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  bool result = start;
  bool any = false;
  Errors errors;
  const auto visit = [&](const Value &value,
                         const bool stated) -> Result<void> {
    if (const auto *boolean = std::get_if<bool>(&value.content)) {
      result = next(result, *boolean);
      any = true;
    } else if (const auto *number = std::get_if<double>(&value.content)) {
      result = next(result, *number != 0);
      any = true;
    } else if (const auto *error = std::get_if<ErrorType>(&value.content)) {
      errors.add(*error);
    } else if (const auto *text = std::get_if<std::string>(&value.content);
               text != nullptr && stated) {
      const Result<ErrorType> read = truth_of_text(*text);
      if (!read.has_value()) {
        return std::unexpected(read.error());
      }
      errors.add(*read);
    }
    return {};
  };
  for (std::size_t i = 0; i < call.size(); ++i) {
    errors = Errors();
    if (const Result<void> walked = call.for_each_value(i, visit);
        !walked.has_value()) {
      return std::unexpected(walked.error());
    }
    const Result<std::optional<ErrorType>> error = errors.first();
    if (!error.has_value()) {
      return std::unexpected(error.error());
    }
    if (error->has_value()) {
      return Value{**error};
    }
  }
  if (!any) {
    return Value{ErrorType::value};
  }
  return Value{result};
}

Result<Value> all(const Call &call) {
  return combine(call, true, [](const bool a, const bool b) { return a && b; });
}
Result<Value> any(const Call &call) {
  return combine(call, false,
                 [](const bool a, const bool b) { return a || b; });
}
Result<Value> one(const Call &call) {
  return combine(call, false,
                 [](const bool a, const bool b) { return a != b; });
}

Result<Value> negation(const Call &call) {
  if (const Result<void> expected = expect_arguments(call, 1, 1);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  const Result<Truth> test = call.scalar(0).and_then(truth);
  if (!test.has_value()) {
    return std::unexpected(test.error());
  }
  if (const auto *error = std::get_if<ErrorType>(&*test)) {
    return Value{*error};
  }
  return Value{!*std::get_if<bool>(&*test)};
}

Result<Value> choose(const Call &call) {
  if (const Result<void> expected = expect_arguments(call, 2, 255);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  const Result<double> read = number_argument(call, 0);
  if (!read.has_value()) {
    return std::unexpected(read.error());
  }
  const double at = std::trunc(*read);
  if (at < 1 || at >= static_cast<double>(call.size())) {
    return refused(call, ErrorType::value);
  }
  return call.value(static_cast<std::size_t>(at));
}

/// A question about the type of a value, which never answers an error.
template <bool (*question)(const Call &, const Value &argument,
                           const Value &value)>
Result<Value> is(const Call &call) {
  if (const Result<void> expected = expect_arguments(call, 1, 1);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  const Result<Value> argument = call.value(0);
  if (!argument.has_value()) {
    return argument;
  }
  return call.scalar_of(*argument).transform([&](const Value &value) {
    return Value{question(call, *argument, value)};
  });
}

bool is_blank(const Call &, const Value &, const Value &value) noexcept {
  return value.holds<Empty>();
}
bool is_number(const Call &call, const Value &, const Value &value) noexcept {
  return value.holds<double>() || (value.holds<bool>() && is_libreoffice(call));
}
bool is_text(const Call &, const Value &, const Value &value) noexcept {
  return value.holds<std::string>();
}
bool is_not_text(const Call &, const Value &, const Value &value) noexcept {
  return !value.holds<std::string>();
}
/// LibreOffice states a boolean cell as a number shown as one, so only a
/// boolean a formula computes is logical there.
bool is_logical(const Call &call, const Value &argument,
                const Value &value) noexcept {
  if (is_libreoffice(call) && argument.holds<Reference>()) {
    return false;
  }
  return value.holds<bool>();
}
bool is_error(const Call &, const Value &, const Value &value) noexcept {
  return value.holds<ErrorType>();
}
bool is_err(const Call &, const Value &, const Value &value) noexcept {
  const auto *error = std::get_if<ErrorType>(&value.content);
  return error != nullptr && *error != ErrorType::not_available;
}
bool is_not_available(const Call &, const Value &,
                      const Value &value) noexcept {
  const auto *error = std::get_if<ErrorType>(&value.content);
  return error != nullptr && *error == ErrorType::not_available;
}

/// `ISEVEN` and `ISODD`. Excel refuses a boolean, with an error its
/// documentation does not name.
template <bool odd> Result<Value> parity(const Call &call) {
  if (const Result<void> expected = expect_arguments(call, 1, 1);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  const Result<Value> value = call.scalar(0);
  if (!value.has_value()) {
    return value;
  }
  if (value->holds<bool>() && !is_libreoffice(call)) {
    return no_answer();
  }
  const Result<Number> number = call.number(*value);
  if (!number.has_value()) {
    return std::unexpected(number.error());
  }
  if (const auto *error = std::get_if<ErrorType>(&*number)) {
    return Value{*error};
  }
  const double whole = std::trunc(*std::get_if<double>(&*number));
  return Value{(std::fmod(std::abs(whole), 2) == 1) == odd};
}

/// `N`: a number as it is, a boolean as 1 or 0, anything else as 0.
Result<Value> number_of(const Call &call) {
  if (const Result<void> expected = expect_arguments(call, 1, 1);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  Result<Value> value = call.scalar(0);
  if (!value.has_value() || value->holds<double>() ||
      value->holds<ErrorType>()) {
    return value;
  }
  if (const auto *boolean = std::get_if<bool>(&value->content)) {
    return Value{*boolean ? 1.0 : 0.0};
  }
  return Value{0.0};
}

/// `T`: a text as it is, anything else as the empty text.
Result<Value> text_of(const Call &call) {
  if (const Result<void> expected = expect_arguments(call, 1, 1);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  Result<Value> value = call.scalar(0);
  if (!value.has_value() || value->holds<std::string>() ||
      value->holds<ErrorType>()) {
    return value;
  }
  return Value{std::string()};
}

/// `CELL`: what a cell is, as the first argument asks: `address`, `col`,
/// `row`, `contents` or `type`. Without a reference LibreOffice reads the
/// formula's own cell, and Excel the cell that changed last.
Result<Value> cell_info(const Call &call) noexcept {
  if (const Result<void> expected = expect_arguments(call, 1, 2);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  const Result<Text> asked = call.scalar(0).and_then(
      [&](const Value &value) { return call.text(value); });
  if (!asked.has_value()) {
    return std::unexpected(asked.error());
  }
  if (const auto *error = std::get_if<ErrorType>(&*asked)) {
    return Value{*error};
  }
  SheetPosition at = call.cell();
  if (call.size() < 2) {
    if (!is_libreoffice(call)) {
      return no_answer();
    }
  } else {
    const Result<Value> value = call.value(1);
    if (!value.has_value()) {
      return value;
    }
    if (const auto *error = std::get_if<ErrorType>(&value->content)) {
      return Value{*error};
    }
    const auto *reference = std::get_if<Reference>(&value->content);
    if (reference == nullptr || reference->areas.size() != 1) {
      return no_answer();
    }
    at = SheetPosition(reference->areas.front().sheet,
                       reference->areas.front().range.from());
  }
  const std::string info =
      util::string::to_lower(*std::get_if<std::string>(&*asked));
  if (info == "address") {
    if (at.sheet != call.cell().sheet) {
      return no_answer(); // the name of the sheet, and in Excel of the file
    }
    return Value{"$" + TablePosition::to_column_string(at.cell.column) + "$" +
                 std::to_string(at.cell.row + 1)};
  }
  if (info == "row" || info == "col") {
    return Value{1.0 + (info == "row" ? at.cell.row : at.cell.column)};
  }
  const Result<Value> value = call.cell_value(at);
  if (!value.has_value()) {
    return value;
  }
  if (info == "contents") {
    if (value->holds<Empty>() && !is_libreoffice(call)) {
      return no_answer(); // LibreOffice gives 0, and Excel does not document it
    }
    return value->holds<Empty>() ? Value{0.0} : *value;
  }
  if (info == "type") {
    if (value->holds<std::string>()) {
      return no_answer(); // `l` for a text, but in Excel `v` for a formula's
    }
    return Value{std::string(value->holds<Empty>() ? "b" : "v")};
  }
  return refused(call, ErrorType::value);
}

constexpr std::array entries{
    FunctionEntry{"AND", all},
    FunctionEntry{"CELL", cell_info},
    FunctionEntry{"CHOOSE", choose},
    FunctionEntry{"IF", condition},
    FunctionEntry{"IFERROR", if_error<false>},
    FunctionEntry{"IFNA", if_error<true>},
    FunctionEntry{"ISBLANK", is<is_blank>},
    FunctionEntry{"ISERR", is<is_err>},
    FunctionEntry{"ISERROR", is<is_error>},
    FunctionEntry{"ISEVEN", parity<false>},
    FunctionEntry{"ISLOGICAL", is<is_logical>},
    FunctionEntry{"ISNA", is<is_not_available>},
    FunctionEntry{"ISNONTEXT", is<is_not_text>},
    FunctionEntry{"ISNUMBER", is<is_number>},
    FunctionEntry{"ISODD", parity<true>},
    FunctionEntry{"ISTEXT", is<is_text>},
    FunctionEntry{"N", number_of},
    FunctionEntry{"NOT", negation},
    FunctionEntry{"OR", any},
    FunctionEntry{"T", text_of},
    FunctionEntry{"XOR", one},
};

} // namespace

} // namespace odr::internal::formula

namespace odr::internal {

std::span<const formula::FunctionEntry> formula::logic_functions() noexcept {
  return entries;
}

} // namespace odr::internal
