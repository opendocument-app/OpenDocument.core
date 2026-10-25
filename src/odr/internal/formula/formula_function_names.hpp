#pragma once

#include <span>
#include <string>
#include <string_view>

namespace odr::internal::formula {

/// The name LibreOffice shows for a function in a language.
struct FunctionName final {
  std::string_view language; ///< as LibreOffice names it: `de`, `pt-BR`
  std::string_view english;
  std::string_view localized;
};

/// Every name a language does not keep in English, of the functions the
/// evaluator implements. `tools/formula/generate_function_names.py` writes it.
[[nodiscard]] std::span<const FunctionName> function_names();

/// Lowercase for the alphabet of the generated names; Turkish I is handled
/// by the locale-aware caller.
[[nodiscard]] char32_t lower_function_letter(char32_t letter);

/// @p formula with each function name spelled as LibreOffice shows it in the
/// language of the BCP 47 tag @p locale. A name the language keeps stays.
[[nodiscard]] std::string localized_functions(std::string_view formula,
                                              std::string_view locale);
/// @p formula with each function name a user of @p locale types spelled in
/// English, as a file states it. An English name stays.
[[nodiscard]] std::string english_functions(std::string_view formula,
                                            std::string_view locale);

} // namespace odr::internal::formula
