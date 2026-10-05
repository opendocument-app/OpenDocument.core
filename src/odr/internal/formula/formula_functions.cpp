#include <odr/internal/formula/formula_function.hpp>

#include <odr/internal/util/string_util.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <span>
#include <string>
#include <unordered_map>

namespace odr::internal::formula {

namespace str = util::string;

namespace {

template <auto value> Value constant(const Call &call) {
  expect_arguments(call, 0, 0);
  return Value{value};
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

void formula::Errors::add(const ErrorType error) {
  if (!m_first.has_value()) {
    m_first = error;
  } else if (*m_first != error) {
    m_mixed = true;
  }
}

std::optional<formula::ErrorType> formula::Errors::first() const {
  if (m_mixed) {
    throw NoAnswer{};
  }
  return m_first;
}

double formula::number_argument(const Call &call, const std::size_t index) {
  const Number number = call.number(call.scalar(index));
  if (const auto *error = std::get_if<ErrorType>(&number)) {
    throw ErrorResult{*error};
  }
  const double value = std::get<double>(number);
  if (!std::isfinite(value)) {
    throw NoAnswer{};
  }
  return value;
}

bool formula::same_number(const Dialect dialect, const double a,
                          const double b) {
  const bool approximately = approximately_equal(a, b);
  // two numbers this far apart differ in 15 digits too
  if (dialect == Dialect::libreoffice || a == b ||
      (!approximately &&
       std::abs(a - b) > std::max(std::abs(a), std::abs(b)) * 1e-13)) {
    return approximately;
  }
  if (approximately != (snapped(a, 15) == snapped(b, 15))) {
    throw NoAnswer{};
  }
  return approximately;
}

bool formula::is_libreoffice(const Call &call) {
  return call.settings().dialect == Dialect::libreoffice;
}

formula::Value formula::refused(const Call &call, const ErrorType excel) {
  if (is_libreoffice(call)) {
    throw NoAnswer{};
  }
  return Value{excel};
}

void formula::expect_arguments(const Call &call, const std::size_t least,
                               const std::size_t most) {
  if (call.size() < least || call.size() > most) {
    throw NoAnswer{};
  }
  for (std::size_t i = 0; i < call.size(); ++i) {
    if (call.missing(i)) {
      throw NoAnswer{};
    }
  }
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

formula::Function formula::find_function(const std::string_view name) {
  const auto found = functions().find(canonical_name(name));
  return found == functions().end() ? nullptr : found->second;
}

bool formula::is_pattern(const Call &call, const std::string_view text) {
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
