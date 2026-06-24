#pragma once

#include <string>

namespace odr::internal::font::type1 {

class Type1Font;

/// Convert a parsed Type1 font to a **bare CFF** font program: translate every
/// glyph's charstring to Type2 (`to_type2`, flattening the font's `/Subrs`) and
/// assemble via the CFF builder, with `.notdef` placed at glyph 0.
///
/// Returns the CFF bytes (not a `cff::CffFont`): a `CffFont` is the
/// parse-and-keep-the-bytes reader, so producing one means parsing this output
/// back — the caller does that (`CffFont{to_cff(font)}`), then `wrap_to_otf`
/// wraps it for the browser, so an embedded Type1 font reuses the entire 3.4
/// CFF path. Mirrors `cff::wrap_to_otf`, which likewise emits bytes.
[[nodiscard]] std::string to_cff(const Type1Font &font);

} // namespace odr::internal::font::type1
