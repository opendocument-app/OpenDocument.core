#pragma once

#include <span>
#include <string>
#include <string_view>

namespace odr::internal::font::type1 {

/// Convert Type1 outlines to Type2, flattening subroutines and dropping hints.
/// Preserves the width; throws on invalid operands or excessive recursion.
[[nodiscard]] std::string to_type2(std::string_view type1,
                                   std::span<const std::string> subrs);

} // namespace odr::internal::font::type1
