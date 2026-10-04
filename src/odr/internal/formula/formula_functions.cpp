#include <odr/internal/formula/formula_function.hpp>

#include <odr/internal/util/string_util.hpp>

#include <algorithm>
#include <span>
#include <string>
#include <unordered_map>

namespace odr::internal::formula {

namespace str = util::string;

namespace {

constexpr FunctionEntry constant_functions[] = {
    {"TRUE", [](const Call &) { return Value{true}; }},
    {"FALSE", [](const Call &) { return Value{false}; }},
    {"NA", [](const Call &) { return Value{ErrorType::not_available}; }},
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
         {std::span<const FunctionEntry>(constant_functions),
          math_functions()}) {
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

formula::Function formula::find_function(const std::string_view name) {
  const auto found = functions().find(canonical_name(name));
  return found == functions().end() ? nullptr : found->second;
}

} // namespace odr::internal
