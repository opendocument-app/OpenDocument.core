#pragma once

#include <span>

namespace odr::internal::util::locale {

/// A letter and its lowercase, where that is one other code point.
struct LowercasePair final {
  char32_t letter{0};
  char32_t lower{0};
};

/// Every such pair of Unicode, sorted by `letter`.
/// `tools/util/generate_lowercase.py` writes it.
[[nodiscard]] std::span<const LowercasePair> lowercase_pairs();

} // namespace odr::internal::util::locale
