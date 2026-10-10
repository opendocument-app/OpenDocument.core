#pragma once

#include <string>

namespace odr::internal::font::type1 {

class Type1Font;

/// Converts Type1 glyphs to bare CFF, flattening Subrs and placing .notdef at
/// 0. Parse with CffFont, then wrap_to_otf for browser embedding.
[[nodiscard]] std::string to_cff(const Type1Font &font);

} // namespace odr::internal::font::type1
