#include <odr/internal/formula/formula_function.hpp>

#include <odr/internal/util/string_util.hpp>

#include <algorithm>
#include <array>
#include <span>
#include <string>
#include <unordered_map>

namespace odr::internal::formula {

namespace str = util::string;

namespace {

constexpr std::array constant_functions{
    FunctionEntry{"TRUE", [](const Call &) { return Value{true}; }},
    FunctionEntry{"FALSE", [](const Call &) { return Value{false}; }},
    FunctionEntry{"NA",
                  [](const Call &) { return Value{ErrorType::not_available}; }},
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

formula::Function formula::find_function(const std::string_view name) {
  const auto found = functions().find(canonical_name(name));
  return found == functions().end() ? nullptr : found->second;
}

} // namespace odr::internal
