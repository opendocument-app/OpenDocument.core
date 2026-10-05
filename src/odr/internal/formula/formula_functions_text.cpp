#include <odr/internal/formula/formula_function.hpp>
#include <odr/internal/formula/formula_text.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <variant>

namespace odr::internal::formula {

namespace {

/// Argument @p index as a text.
std::string string_argument(const Call &call, const std::size_t index) {
  Text text = call.text(call.scalar(index));
  if (const auto *error = std::get_if<ErrorType>(&text)) {
    throw ErrorResult{*error};
  }
  return std::move(std::get<std::string>(text));
}

/// Argument @p index as a text in UTF-16.
std::u16string text_argument(const Call &call, const std::size_t index) {
  std::optional<std::u16string> units = utf16_of(string_argument(call, index));
  if (!units.has_value()) {
    throw NoAnswer{};
  }
  return std::move(*units);
}

/// Argument @p index as a whole number.
double count_argument(const Call &call, const std::size_t index) {
  return std::trunc(number_argument(call, index));
}

Value text_value(const std::u16string &text) {
  if (text.size() > text_limit) {
    throw NoAnswer{};
  }
  return Value{utf8_of(text)};
}

Value length(const Call &call) {
  expect_arguments(call, 1, 1);
  const std::u16string text = text_argument(call, 0);
  return Value{static_cast<double>(text.size())};
}

/// `LEFT` and `RIGHT`: the first or the last @p count units.
template <bool from_end> Value side(const Call &call) {
  expect_arguments(call, 1, 2);
  const std::u16string text = text_argument(call, 0);
  const double count = call.size() > 1 ? count_argument(call, 1) : 1;
  if (count < 0) {
    return refused(call, ErrorType::value);
  }
  const std::size_t take =
      std::min(text.size(), static_cast<std::size_t>(std::min(count, 1e9)));
  return text_value(from_end ? text.substr(text.size() - take)
                             : text.substr(0, take));
}

Value middle(const Call &call) {
  expect_arguments(call, 3, 3);
  const std::u16string text = text_argument(call, 0);
  const double start = count_argument(call, 1);
  const double count = count_argument(call, 2);
  if (start < 1 || count < 0) {
    return refused(call, ErrorType::value);
  }
  if (start > static_cast<double>(text.size())) {
    return Value{std::string()};
  }
  const auto from = static_cast<std::size_t>(start) - 1;
  return text_value(
      text.substr(from, static_cast<std::size_t>(std::min(count, 1e9))));
}

template <bool upper> Value change_case(const Call &call) {
  expect_arguments(call, 1, 1);
  const std::u16string text = text_argument(call, 0);
  const std::optional<std::u16string> result =
      upper ? to_upper(text) : to_lower(text);
  if (!result.has_value()) {
    throw NoAnswer{};
  }
  return text_value(*result);
}

/// `TRIM`: no space at either end, and one between words.
Value trim(const Call &call) {
  expect_arguments(call, 1, 1);
  const std::u16string text = text_argument(call, 0);
  std::u16string result;
  bool space = false;
  for (const char16_t c : text) {
    if (c == u' ') {
      space = !result.empty();
      continue;
    }
    if (space) {
      result += u' ';
      space = false;
    }
    result += c;
  }
  return text_value(result);
}

Value concatenate(const Call &call) {
  expect_arguments(call, 1, 255);
  std::u16string result;
  for (std::size_t i = 0; i < call.size(); ++i) {
    const std::u16string text = text_argument(call, i);
    result += text;
  }
  return text_value(result);
}

/// `CONCAT`: as `CONCATENATE`, but a range adds every cell, row by row.
Value concat(const Call &call) {
  expect_arguments(call, 1, 255);
  std::u16string result;
  for (std::size_t i = 0; i < call.size(); ++i) {
    Errors errors;
    call.for_each_value(i, [&](const Value &value, bool) {
      const Text text = call.text(value);
      if (const auto *error = std::get_if<ErrorType>(&text)) {
        errors.add(*error);
        return;
      }
      const std::optional<std::u16string> units =
          utf16_of(std::get<std::string>(text));
      if (!units.has_value()) {
        throw NoAnswer{};
      }
      result += *units;
      if (result.size() > text_limit) {
        throw NoAnswer{};
      }
    });
    if (const std::optional<ErrorType> error = errors.first()) {
      return Value{*error};
    }
  }
  return text_value(result);
}

Value repeat(const Call &call) {
  expect_arguments(call, 2, 2);
  const std::u16string text = text_argument(call, 0);
  const double count = count_argument(call, 1);
  if (count < 0) {
    return refused(call, ErrorType::value);
  }
  if (text.empty() || count == 0) {
    return Value{std::string()};
  }
  if (count * static_cast<double>(text.size()) > text_limit) {
    throw NoAnswer{};
  }
  std::u16string result;
  for (auto i = static_cast<std::uint32_t>(count); i > 0; --i) {
    result += text;
  }
  return text_value(result);
}

/// `FIND` and `SEARCH`: the position of the first match from the start on,
/// counted from 1. `SEARCH` ignores case.
template <bool ignore_case> Value find(const Call &call) {
  expect_arguments(call, 2, 3);
  const std::u16string needle = text_argument(call, 0);
  const std::u16string haystack = text_argument(call, 1);
  const double start = call.size() > 2 ? count_argument(call, 2) : 1;
  if (start < 1 || start > static_cast<double>(haystack.size()) + 1) {
    return Value{ErrorType::value};
  }
  if (needle.empty()) {
    // LibreOffice refuses to find nothing, Excel finds it at the start
    return is_libreoffice(call) ? Value{ErrorType::value} : Value{start};
  }
  std::u16string pattern = needle;
  std::u16string text = haystack;
  if (ignore_case) {
    if (is_pattern(call, utf8_of(needle))) {
      throw NoAnswer{};
    }
    const std::optional<std::u16string> lower_needle = to_lower(needle);
    const std::optional<std::u16string> lower_text = to_lower(haystack);
    if (!lower_needle.has_value() || !lower_text.has_value()) {
      throw NoAnswer{};
    }
    pattern = *lower_needle;
    text = *lower_text;
  }
  const std::size_t at =
      text.find(pattern, static_cast<std::size_t>(start) - 1);
  if (at == std::u16string::npos) {
    return Value{ErrorType::value};
  }
  return Value{static_cast<double>(at + 1)};
}

Value substitute(const Call &call) {
  expect_arguments(call, 3, 4);
  const std::u16string text = text_argument(call, 0);
  const std::u16string old_text = text_argument(call, 1);
  const std::u16string new_text = text_argument(call, 2);
  double which = 0;
  if (call.size() > 3) {
    const double stated = count_argument(call, 3);
    if (stated < 1) {
      return refused(call, ErrorType::value);
    }
    which = stated;
  }
  if (old_text.empty()) {
    return text_value(text);
  }
  std::u16string result;
  std::size_t from = 0;
  double seen = 0;
  while (true) {
    const std::size_t at = text.find(old_text, from);
    if (at == std::u16string::npos) {
      break;
    }
    ++seen;
    result += text.substr(from, at - from);
    result += which == 0 || seen == which ? new_text : old_text;
    from = at + old_text.size();
    if (result.size() > text_limit) {
      throw NoAnswer{};
    }
  }
  result += text.substr(from);
  return text_value(result);
}

Value replace(const Call &call) {
  expect_arguments(call, 4, 4);
  const std::u16string text = text_argument(call, 0);
  const double start = count_argument(call, 1);
  const double count = count_argument(call, 2);
  const std::u16string new_text = text_argument(call, 3);
  if (start < 1 || count < 0) {
    return refused(call, ErrorType::value);
  }
  const std::size_t from =
      std::min(text.size(), static_cast<std::size_t>(std::min(start, 1e9)) - 1);
  const std::size_t removed = std::min(
      text.size() - from, static_cast<std::size_t>(std::min(count, 1e9)));
  return text_value(text.substr(0, from) + new_text +
                    text.substr(from + removed));
}

Value exact(const Call &call) {
  expect_arguments(call, 2, 2);
  const std::string a = string_argument(call, 0);
  const std::string b = string_argument(call, 1);
  return Value{a == b};
}

/// `VALUE`: the number a text reads as.
Value value(const Call &call) {
  expect_arguments(call, 1, 1);
  Value argument = call.scalar(0);
  if (argument.holds<double>() || argument.holds<ErrorType>()) {
    return argument;
  }
  const auto *text = std::get_if<std::string>(&argument.content);
  if (text == nullptr) {
    throw NoAnswer{};
  }
  const std::optional<Number> number = number_of_text(*text);
  if (!number.has_value()) {
    throw NoAnswer{};
  }
  if (std::holds_alternative<ErrorType>(*number)) {
    return refused(call, ErrorType::value);
  }
  return Value{std::get<double>(*number)};
}

/// `CHAR`: an ASCII character. The applications map the codes past it by
/// different code pages.
Value character(const Call &call) {
  expect_arguments(call, 1, 1);
  const double code = count_argument(call, 0);
  if (code < 1 || code > 255) {
    return refused(call, ErrorType::value);
  }
  if (code > 127) {
    throw NoAnswer{};
  }
  return Value{std::string(1, static_cast<char>(code))};
}

Value code(const Call &call) {
  expect_arguments(call, 1, 1);
  const std::u16string text = text_argument(call, 0);
  if (text.empty()) {
    return refused(call, ErrorType::value);
  }
  if (text.front() > 127) {
    throw NoAnswer{};
  }
  return Value{static_cast<double>(text.front())};
}

constexpr std::array entries{
    FunctionEntry{"CHAR", character},
    FunctionEntry{"CODE", code},
    FunctionEntry{"CONCAT", concat},
    FunctionEntry{"CONCATENATE", concatenate},
    FunctionEntry{"EXACT", exact},
    FunctionEntry{"FIND", find<false>},
    FunctionEntry{"LEFT", side<false>},
    FunctionEntry{"LEN", length},
    FunctionEntry{"LOWER", change_case<false>},
    FunctionEntry{"MID", middle},
    FunctionEntry{"REPLACE", replace},
    FunctionEntry{"REPT", repeat},
    FunctionEntry{"RIGHT", side<true>},
    FunctionEntry{"SEARCH", find<true>},
    FunctionEntry{"SUBSTITUTE", substitute},
    FunctionEntry{"TRIM", trim},
    FunctionEntry{"UPPER", change_case<true>},
    FunctionEntry{"VALUE", value},
};

} // namespace

} // namespace odr::internal::formula

namespace odr::internal {

std::span<const formula::FunctionEntry> formula::text_functions() {
  return entries;
}

} // namespace odr::internal
