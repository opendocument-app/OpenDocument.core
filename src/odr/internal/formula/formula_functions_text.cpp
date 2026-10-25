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
Result<std::string> string_argument(const Call &call,
                                    const std::size_t index) noexcept {
  Result<Text> text = call.scalar(index).and_then(
      [&](const Value &value) { return call.text(value); });
  if (!text.has_value()) {
    return std::unexpected(text.error());
  }
  if (const auto *error = std::get_if<ErrorType>(&*text)) {
    return error_result(*error);
  }
  return std::move(*std::get_if<std::string>(&*text));
}

/// Argument @p index as a text in UTF-16.
Result<std::u16string> text_argument(const Call &call,
                                     const std::size_t index) noexcept {
  const Result<std::string> text = string_argument(call, index);
  if (!text.has_value()) {
    return std::unexpected(text.error());
  }
  std::optional<std::u16string> units = utf16_of(*text);
  if (!units.has_value()) {
    return no_answer();
  }
  return std::move(*units);
}

/// Argument @p index as a whole number.
Result<double> count_argument(const Call &call,
                              const std::size_t index) noexcept {
  return number_argument(call, index).transform([](const double x) {
    return std::trunc(x);
  });
}

Result<Value> text_value(const std::u16string &text) {
  if (text.size() > text_limit) {
    return no_answer();
  }
  return Value{utf8_of(text)};
}

Result<Value> length(const Call &call) noexcept {
  if (const Result<void> expected = expect_arguments(call, 1, 1);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  return text_argument(call, 0).transform([](const std::u16string &text) {
    return Value{static_cast<double>(text.size())};
  });
}

/// `LEFT` and `RIGHT`: the first or the last @p count units.
template <bool from_end> Result<Value> side(const Call &call) {
  if (const Result<void> expected = expect_arguments(call, 1, 2);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  const Result<std::u16string> text = text_argument(call, 0);
  if (!text.has_value()) {
    return std::unexpected(text.error());
  }
  double count = 1;
  if (call.size() > 1) {
    const Result<double> stated = count_argument(call, 1);
    if (!stated.has_value()) {
      return std::unexpected(stated.error());
    }
    count = *stated;
  }
  if (count < 0) {
    return refused(call, ErrorType::value);
  }
  const std::size_t take =
      std::min(text->size(), static_cast<std::size_t>(std::min(count, 1e9)));
  return text_value(from_end ? text->substr(text->size() - take)
                             : text->substr(0, take));
}

Result<Value> middle(const Call &call) {
  if (const Result<void> expected = expect_arguments(call, 3, 3);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  const Result<std::u16string> text = text_argument(call, 0);
  if (!text.has_value()) {
    return std::unexpected(text.error());
  }
  const Result<double> start = count_argument(call, 1);
  if (!start.has_value()) {
    return std::unexpected(start.error());
  }
  const Result<double> count = count_argument(call, 2);
  if (!count.has_value()) {
    return std::unexpected(count.error());
  }
  if (*start < 1 || *count < 0) {
    return refused(call, ErrorType::value);
  }
  if (*start > static_cast<double>(text->size())) {
    return Value{std::string()};
  }
  const auto from = static_cast<std::size_t>(*start) - 1;
  return text_value(
      text->substr(from, static_cast<std::size_t>(std::min(*count, 1e9))));
}

template <bool upper> Result<Value> change_case(const Call &call) noexcept {
  if (const Result<void> expected = expect_arguments(call, 1, 1);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  const Result<std::u16string> text = text_argument(call, 0);
  if (!text.has_value()) {
    return std::unexpected(text.error());
  }
  const std::optional<std::u16string> result =
      upper ? to_upper(*text) : to_lower(*text);
  if (!result.has_value()) {
    return no_answer();
  }
  return text_value(*result);
}

/// `TRIM`: no space at either end, and one between words.
Result<Value> trim(const Call &call) {
  if (const Result<void> expected = expect_arguments(call, 1, 1);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  const Result<std::u16string> text = text_argument(call, 0);
  if (!text.has_value()) {
    return std::unexpected(text.error());
  }
  std::u16string result;
  bool space = false;
  for (const char16_t c : *text) {
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

Result<Value> concatenate(const Call &call) {
  if (const Result<void> expected = expect_arguments(call, 1, 255);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  std::u16string result;
  for (std::size_t i = 0; i < call.size(); ++i) {
    const Result<std::u16string> text = text_argument(call, i);
    if (!text.has_value()) {
      return std::unexpected(text.error());
    }
    result += *text;
  }
  return text_value(result);
}

/// `CONCAT`: as `CONCATENATE`, but a range adds every cell, row by row.
Result<Value> concat(const Call &call) noexcept {
  if (const Result<void> expected = expect_arguments(call, 1, 255);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  std::u16string result;
  Errors errors;
  const auto visit = [&](const Value &value, bool) -> Result<void> {
    const Result<Text> text = call.text(value);
    if (!text.has_value()) {
      return std::unexpected(text.error());
    }
    if (const auto *error = std::get_if<ErrorType>(&*text)) {
      errors.add(*error);
      return {};
    }
    const std::optional<std::u16string> units =
        utf16_of(*std::get_if<std::string>(&*text));
    if (!units.has_value()) {
      return no_answer();
    }
    result += *units;
    if (result.size() > text_limit) {
      return no_answer();
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
  return text_value(result);
}

Result<Value> repeat(const Call &call) {
  if (const Result<void> expected = expect_arguments(call, 2, 2);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  const Result<std::u16string> text = text_argument(call, 0);
  if (!text.has_value()) {
    return std::unexpected(text.error());
  }
  const Result<double> count = count_argument(call, 1);
  if (!count.has_value()) {
    return std::unexpected(count.error());
  }
  if (*count < 0) {
    return refused(call, ErrorType::value);
  }
  if (text->empty() || *count == 0) {
    return Value{std::string()};
  }
  if (*count * static_cast<double>(text->size()) > text_limit) {
    return no_answer();
  }
  std::u16string result;
  for (auto i = static_cast<std::uint32_t>(*count); i > 0; --i) {
    result += *text;
  }
  return text_value(result);
}

/// `FIND` and `SEARCH`: the position of the first match from the start on,
/// counted from 1. `SEARCH` ignores case.
template <bool ignore_case> Result<Value> find(const Call &call) {
  if (const Result<void> expected = expect_arguments(call, 2, 3);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  const Result<std::u16string> needle = text_argument(call, 0);
  if (!needle.has_value()) {
    return std::unexpected(needle.error());
  }
  const Result<std::u16string> haystack = text_argument(call, 1);
  if (!haystack.has_value()) {
    return std::unexpected(haystack.error());
  }
  double start = 1;
  if (call.size() > 2) {
    const Result<double> stated = count_argument(call, 2);
    if (!stated.has_value()) {
      return std::unexpected(stated.error());
    }
    start = *stated;
  }
  if (start < 1 || start > static_cast<double>(haystack->size()) + 1) {
    return Value{ErrorType::value};
  }
  if (needle->empty()) {
    // LibreOffice refuses to find nothing, Excel finds it at the start
    return is_libreoffice(call) ? Value{ErrorType::value} : Value{start};
  }
  std::u16string pattern = *needle;
  std::u16string text = *haystack;
  if (ignore_case) {
    if (is_pattern(call, utf8_of(*needle))) {
      return no_answer();
    }
    const std::optional<std::u16string> lower_needle = to_lower(*needle);
    const std::optional<std::u16string> lower_text = to_lower(*haystack);
    if (!lower_needle.has_value() || !lower_text.has_value()) {
      return no_answer();
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

Result<Value> substitute(const Call &call) {
  if (const Result<void> expected = expect_arguments(call, 3, 4);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  const Result<std::u16string> text = text_argument(call, 0);
  if (!text.has_value()) {
    return std::unexpected(text.error());
  }
  const Result<std::u16string> old_text = text_argument(call, 1);
  if (!old_text.has_value()) {
    return std::unexpected(old_text.error());
  }
  const Result<std::u16string> new_text = text_argument(call, 2);
  if (!new_text.has_value()) {
    return std::unexpected(new_text.error());
  }
  double which = 0;
  if (call.size() > 3) {
    const Result<double> stated = count_argument(call, 3);
    if (!stated.has_value()) {
      return std::unexpected(stated.error());
    }
    if (*stated < 1) {
      return refused(call, ErrorType::value);
    }
    which = *stated;
  }
  if (old_text->empty()) {
    return text_value(*text);
  }
  std::u16string result;
  std::size_t from = 0;
  double seen = 0;
  while (true) {
    const std::size_t at = text->find(*old_text, from);
    if (at == std::u16string::npos) {
      break;
    }
    ++seen;
    result += text->substr(from, at - from);
    result += which == 0 || seen == which ? *new_text : *old_text;
    from = at + old_text->size();
    if (result.size() > text_limit) {
      return no_answer();
    }
  }
  result += text->substr(from);
  return text_value(result);
}

Result<Value> replace(const Call &call) {
  if (const Result<void> expected = expect_arguments(call, 4, 4);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  const Result<std::u16string> text = text_argument(call, 0);
  if (!text.has_value()) {
    return std::unexpected(text.error());
  }
  const Result<double> start = count_argument(call, 1);
  if (!start.has_value()) {
    return std::unexpected(start.error());
  }
  const Result<double> count = count_argument(call, 2);
  if (!count.has_value()) {
    return std::unexpected(count.error());
  }
  const Result<std::u16string> new_text = text_argument(call, 3);
  if (!new_text.has_value()) {
    return std::unexpected(new_text.error());
  }
  if (*start < 1 || *count < 0) {
    return refused(call, ErrorType::value);
  }
  const std::size_t from = std::min(
      text->size(), static_cast<std::size_t>(std::min(*start, 1e9)) - 1);
  const std::size_t removed = std::min(
      text->size() - from, static_cast<std::size_t>(std::min(*count, 1e9)));
  return text_value(text->substr(0, from) + *new_text +
                    text->substr(from + removed));
}

Result<Value> exact(const Call &call) noexcept {
  if (const Result<void> expected = expect_arguments(call, 2, 2);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  const Result<std::string> a = string_argument(call, 0);
  if (!a.has_value()) {
    return std::unexpected(a.error());
  }
  const Result<std::string> b = string_argument(call, 1);
  if (!b.has_value()) {
    return std::unexpected(b.error());
  }
  return Value{*a == *b};
}

/// `VALUE`: the number a text reads as.
Result<Value> value(const Call &call) noexcept {
  if (const Result<void> expected = expect_arguments(call, 1, 1);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  Result<Value> argument = call.scalar(0);
  if (!argument.has_value() || argument->holds<double>() ||
      argument->holds<ErrorType>()) {
    return argument;
  }
  const auto *text = std::get_if<std::string>(&argument->content);
  if (text == nullptr) {
    return no_answer();
  }
  const std::optional<Number> number = number_of_text(*text);
  if (!number.has_value()) {
    return no_answer();
  }
  const auto *read = std::get_if<double>(&*number);
  if (read == nullptr) {
    return refused(call, ErrorType::value);
  }
  return Value{*read};
}

/// `CHAR`: an ASCII character. The applications map the codes past it by
/// different code pages.
Result<Value> character(const Call &call) {
  if (const Result<void> expected = expect_arguments(call, 1, 1);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  const Result<double> code = count_argument(call, 0);
  if (!code.has_value()) {
    return std::unexpected(code.error());
  }
  if (*code < 1 || *code > 255) {
    return refused(call, ErrorType::value);
  }
  if (*code > 127) {
    return no_answer();
  }
  return Value{std::string(1, static_cast<char>(*code))};
}

Result<Value> code(const Call &call) noexcept {
  if (const Result<void> expected = expect_arguments(call, 1, 1);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  const Result<std::u16string> text = text_argument(call, 0);
  if (!text.has_value()) {
    return std::unexpected(text.error());
  }
  if (text->empty()) {
    return refused(call, ErrorType::value);
  }
  if (text->front() > 127) {
    return no_answer();
  }
  return Value{static_cast<double>(text->front())};
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
