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

} // namespace odr::internal::util::locale
