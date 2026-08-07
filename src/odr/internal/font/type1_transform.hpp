#pragma once

#include <string>

namespace odr::internal::font::type1 {

class Type1Font;

/// Convert a parsed Type1 font to a **bare CFF** font program: translate every
/// glyph's charstring to Type2 (`to_type2`, flattening the font's `/Subrs`) and
/// assemble via the CFF builder, with `.notdef` placed at glyph 0.
///
/// Returns the CFF bytes, not a `cff::CffFont`: the caller parses them back
/// (`CffFont{to_cff(font)}`) and hands the result to `wrap_to_otf`, so an
/// embedded Type1 font reuses the whole CFF path.
[[nodiscard]] std::string to_cff(const Type1Font &font);

} // namespace odr::internal::font::type1
