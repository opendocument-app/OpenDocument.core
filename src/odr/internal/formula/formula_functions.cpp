#include <odr/internal/formula/formula_function.hpp>

#include <odr/internal/util/string_util.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <span>
#include <string>
#include <unordered_map>

namespace odr::internal::formula {

namespace str = util::string;

namespace {

template <auto value> Result<Value> constant(const Call &call) noexcept {
  return expect_arguments(call, 0, 0).transform([] { return Value{value}; });
}

constexpr std::array constant_functions{
    FunctionEntry{"TRUE", constant<true>},
    FunctionEntry{"FALSE", constant<false>},
    FunctionEntry{"NA", constant<ErrorType::not_available>},
};

/// The function a name stands for once the prefix of its format is gone.
std::string canonical_name(std::string_view name) {
  for (const std::string_view prefix :
       {"_xlfn._xlws.", "_xlfn.", "_xlws.", "com.microsoft.", "org.openoffice.",
        "org.libreoffice."}) {
    if (name.size() > prefix.size() &&
        str::equals_ignore_case(name.substr(0, prefix.size()), prefix)) {
      name.remove_prefix(prefix.size());
      break;
    }
  }
  std::string result(name);
  std::ranges::transform(result, result.begin(), str::to_upper);
  return result;
}

const std::unordered_map<std::string, Function> &functions() {
  static const std::unordered_map<std::string, Function> result = [] {
    std::unordered_map<std::string, Function> table;
    for (const std::span<const FunctionEntry> entries :
         {std::span<const FunctionEntry>(constant_functions), math_functions(),
          logic_functions(), text_functions(), lookup_functions(),
          date_functions()}) {
      for (const FunctionEntry &entry : entries) {
        table.emplace(entry.name, entry.function);
      }
    }
    return table;
  }();
  return result;
}

} // namespace

} // namespace odr::internal::formula

namespace odr::internal {

void formula::Errors::add(const ErrorType error) noexcept {
  if (!m_first.has_value()) {
    m_first = error;
  } else if (*m_first != error) {
    m_mixed = true;
  }
}

formula::Result<std::optional<formula::ErrorType>>
formula::Errors::first() const noexcept {
  if (m_mixed) {
    return no_answer();
  }
  return m_first;
}

formula::Result<double>
formula::number_argument(const Call &call, const std::size_t index) noexcept {
  const Result<Number> number = call.scalar(index).and_then(
      [&](const Value &value) { return call.number(value); });
  if (!number.has_value()) {
    return std::unexpected(number.error());
  }
  if (const auto *error = std::get_if<ErrorType>(&*number)) {
    return error_result(*error);
  }
  const double value = *std::get_if<double>(&*number);
  if (!std::isfinite(value)) {
    return no_answer();
  }
  return value;
}

formula::Result<bool> formula::same_number(const Dialect dialect,
                                           const double a,
                                           const double b) noexcept {
  const bool approximately = approximately_equal(a, b);
  // two numbers this far apart differ in 15 digits too
  if (dialect == Dialect::libreoffice || a == b ||
      (!approximately &&
       std::abs(a - b) > std::max(std::abs(a), std::abs(b)) * 1e-13)) {
    return approximately;
  }
  if (approximately != (snapped(a, 15) == snapped(b, 15))) {
    return no_answer();
  }
  return approximately;
}

bool formula::is_libreoffice(const Call &call) noexcept {
  return call.settings().dialect == Dialect::libreoffice;
}

formula::Result<formula::Value>
formula::refused(const Call &call, const ErrorType excel) noexcept {
  if (is_libreoffice(call)) {
    return no_answer();
  }
  return Value{excel};
}

formula::Result<void>
formula::expect_arguments(const Call &call, const std::size_t least,
                          const std::size_t most) noexcept {
  if (call.size() < least || call.size() > most) {
    return no_answer();
  }
  for (std::size_t i = 0; i < call.size(); ++i) {
    if (call.missing(i)) {
      return no_answer();
    }
  }
  return {};
}

formula::ArrayRule formula::find_array_rule(const std::string_view name) {
  const std::string canonical = canonical_name(name);
  const auto in = [&](const std::initializer_list<std::string_view> names) {
    return std::ranges::find(names, canonical) != names.end();
  };
  if (in({"OFFSET", "INDIRECT", "CELL"})) {
    return nullptr;
  }
  if (in({"SUM",    "PRODUCT", "AVERAGE", "COUNT",     "COUNTA",  "MAX",
          "MIN",    "MEDIAN",  "SUMSQ",   "STDEV",     "STDEV.P", "STDEV.S",
          "STDEVP", "VAR",     "VAR.P",   "VAR.S",     "VARP",    "AND",
          "OR",     "XOR",     "CONCAT",  "SUMPRODUCT"})) {
    return [](std::size_t) { return true; };
  }
  if (in({"INDEX", "LARGE", "SMALL", "ROWS", "COLUMNS", "COUNTIF",
          "COUNTBLANK"})) {
    return [](const std::size_t index) { return index == 0; };
  }
  if (in({"MATCH", "VLOOKUP", "HLOOKUP"})) {
    return [](const std::size_t index) { return index == 1; };
  }
  if (in({"SUMIF", "AVERAGEIF"})) {
    return [](const std::size_t index) { return index == 0 || index == 2; };
  }
  if (in({"COUNTIFS"})) {
    return [](const std::size_t index) { return index % 2 == 0; };
  }
  if (in({"SUMIFS", "AVERAGEIFS"})) {
    return [](const std::size_t index) { return index == 0 || index % 2 == 1; };
  }
  return [](std::size_t) { return false; };
}

bool formula::is_volatile(const Node &node) {
  if (const auto *call = std::get_if<FunctionCall>(&node.content)) {
    const std::string name = canonical_name(call->name);
    for (const std::string_view volatile_name :
         {"RAND", "RANDBETWEEN", "NOW", "TODAY", "OFFSET", "INDIRECT", "CELL",
          "INFO"}) {
      if (name == volatile_name) {
        return true;
      }
    }
  }
  return std::ranges::any_of(node.children, is_volatile);
}

bool formula::has_dynamic_references(const Node &node) {
  if (const auto *call = std::get_if<FunctionCall>(&node.content)) {
    const std::string name = canonical_name(call->name);
    if (name == "OFFSET" || name == "INDIRECT") {
      return true;
    }
  }
  return std::ranges::any_of(node.children, has_dynamic_references);
}

formula::Function formula::find_function(const std::string_view name) {
  const auto found = functions().find(canonical_name(name));
  return found == functions().end() ? nullptr : found->second;
}

bool formula::is_pattern(const Call &call,
                         const std::string_view text) noexcept {
  const bool wildcards = !is_libreoffice(call) || call.settings().wildcards;
  const bool expressions =
      is_libreoffice(call) && call.settings().regular_expressions;
  for (const char c : text) {
    if (wildcards && (c == '*' || c == '?' || c == '~')) {
      return true;
    }
    if (expressions &&
        std::string_view(".^$*+?()[]{}|\\").find(c) != std::string_view::npos) {
      return true;
    }
  }
  return false;
}

} // namespace odr::internal
