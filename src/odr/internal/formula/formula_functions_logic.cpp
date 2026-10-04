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
Truth truth_of_text(const std::string &text) {
  if (util::string::equals_ignore_case(text, "TRUE") ||
      util::string::equals_ignore_case(text, "FALSE")) {
    throw NoAnswer{};
  }
  return ErrorType::value;
}

/// @p value read as a condition: an empty cell is false, a number true
/// unless it is 0.
Truth truth(const Value &value) {
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
  return truth_of_text(value.get<std::string>());
}

Value condition(const Call &call) {
  if (call.size() < 1 || call.size() > 3 || call.missing(0)) {
    throw NoAnswer{};
  }
  const Truth test = truth(call.scalar(0));
  if (const auto *error = std::get_if<ErrorType>(&test)) {
    return Value{*error};
  }
  const std::size_t branch = std::get<bool>(test) ? 1 : 2;
  if (branch >= call.size()) {
    // `IF(c)` and `IF(c; x)` give the condition where no branch is stated
    return Value{std::get<bool>(test)};
  }
  if (call.missing(branch)) {
    return Value{0.0};
  }
  return call.value(branch);
}

/// `IFERROR` and `IFNA`: the second argument where the first is an error
/// @p catches answers to.
template <bool only_not_available> Value if_error(const Call &call) {
  expect_arguments(call, 2, 2);
  const Value value = call.scalar(0);
  const auto *error = std::get_if<ErrorType>(&value.content);
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
Value combine(const Call &call, const bool start, const Combine &next) {
  expect_arguments(call, 1, 255);
  bool result = start;
  bool any = false;
  for (std::size_t i = 0; i < call.size(); ++i) {
    Errors errors;
    call.for_each_value(i, [&](const Value &value, const bool stated) {
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
        errors.add(std::get<ErrorType>(truth_of_text(*text)));
      }
    });
    if (const std::optional<ErrorType> error = errors.first()) {
      return Value{*error};
    }
  }
  if (!any) {
    return Value{ErrorType::value};
  }
  return Value{result};
}

Value all(const Call &call) {
  return combine(call, true, [](const bool a, const bool b) { return a && b; });
}
Value any(const Call &call) {
  return combine(call, false,
                 [](const bool a, const bool b) { return a || b; });
}
Value one(const Call &call) {
  return combine(call, false,
                 [](const bool a, const bool b) { return a != b; });
}

Value negation(const Call &call) {
  expect_arguments(call, 1, 1);
  const Truth test = truth(call.scalar(0));
  if (const auto *error = std::get_if<ErrorType>(&test)) {
    return Value{*error};
  }
  return Value{!std::get<bool>(test)};
}

Value choose(const Call &call) {
  expect_arguments(call, 2, 255);
  const double at = std::trunc(number_argument(call, 0));
  if (at < 1 || at >= static_cast<double>(call.size())) {
    return refused(call, ErrorType::value);
  }
  return call.value(static_cast<std::size_t>(at));
}

/// A question about the type of a value, which never answers an error.
template <bool (*question)(const Call &, const Value &argument,
                           const Value &value)>
Value is(const Call &call) {
  expect_arguments(call, 1, 1);
  const Value argument = call.value(0);
  return Value{question(call, argument, call.scalar_of(argument))};
}

bool is_blank(const Call &, const Value &, const Value &value) {
  return value.holds<Empty>();
}
bool is_number(const Call &call, const Value &, const Value &value) {
  return value.holds<double>() || (value.holds<bool>() && is_libreoffice(call));
}
bool is_text(const Call &, const Value &, const Value &value) {
  return value.holds<std::string>();
}
bool is_not_text(const Call &, const Value &, const Value &value) {
  return !value.holds<std::string>();
}
/// LibreOffice states a boolean cell as a number shown as one, so only a
/// boolean a formula computes is logical there.
bool is_logical(const Call &call, const Value &argument, const Value &value) {
  if (is_libreoffice(call) && argument.holds<Reference>()) {
    return false;
  }
  return value.holds<bool>();
}
bool is_error(const Call &, const Value &, const Value &value) {
  return value.holds<ErrorType>();
}
bool is_err(const Call &, const Value &, const Value &value) {
  return value.holds<ErrorType>() &&
         value.get<ErrorType>() != ErrorType::not_available;
}
bool is_not_available(const Call &, const Value &, const Value &value) {
  return value.holds<ErrorType>() &&
         value.get<ErrorType>() == ErrorType::not_available;
}

/// `ISEVEN` and `ISODD`. Excel refuses a boolean, with an error its
/// documentation does not name.
template <bool odd> Value parity(const Call &call) {
  expect_arguments(call, 1, 1);
  const Value value = call.scalar(0);
  if (value.holds<bool>() && !is_libreoffice(call)) {
    throw NoAnswer{};
  }
  const Number number = call.number(value);
  if (const auto *error = std::get_if<ErrorType>(&number)) {
    return Value{*error};
  }
  const double whole = std::trunc(std::get<double>(number));
  return Value{(std::fmod(std::abs(whole), 2) == 1) == odd};
}

/// `N`: a number as it is, a boolean as 1 or 0, anything else as 0.
Value number_of(const Call &call) {
  expect_arguments(call, 1, 1);
  const Value value = call.scalar(0);
  if (value.holds<double>() || value.holds<ErrorType>()) {
    return value;
  }
  if (const auto *boolean = std::get_if<bool>(&value.content)) {
    return Value{*boolean ? 1.0 : 0.0};
  }
  return Value{0.0};
}

/// `T`: a text as it is, anything else as the empty text.
Value text_of(const Call &call) {
  expect_arguments(call, 1, 1);
  const Value value = call.scalar(0);
  if (value.holds<std::string>() || value.holds<ErrorType>()) {
    return value;
  }
  return Value{std::string()};
}

constexpr std::array entries{
    FunctionEntry{"AND", all},
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

std::span<const formula::FunctionEntry> formula::logic_functions() {
  return entries;
}

} // namespace odr::internal
