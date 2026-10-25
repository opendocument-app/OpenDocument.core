#pragma once

#include <string>
#include <string_view>

namespace odr::internal::util::locale {

/// The subtags of a BCP 47 language tag that the tables of languages ask for.
struct LanguageTag final {
  std::string language; ///< lowercase: `de`; `no` is read as `nb`
  std::string script;   ///< title case: `Latn`; empty where none is stated
  std::string region;   ///< uppercase: `DE`, `419`; empty where none is stated

  /// @p tag with `-` or `_` between its subtags, in any case. A variant and
  /// what follows a singleton are dropped.
  [[nodiscard]] static LanguageTag parse(std::string_view tag);
};

/// @p letter in lowercase, with the rules of @p language, a lowercase BCP 47
/// language subtag: in `tr` and `az`, `I` is a dotless `ı`. A letter whose
/// lowercase is more than one code point takes its simple lowercase.
[[nodiscard]] char32_t to_lower(char32_t letter, std::string_view language);

/// Whether the UTF-8 strings @p a and @p b are the same in lowercase, with the
/// rules of @p language.
[[nodiscard]] bool equals_ignore_case(std::string_view a, std::string_view b,
                                      std::string_view language);

} // namespace odr::internal::util::locale
