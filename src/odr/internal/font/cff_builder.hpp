#pragma once

#include <odr/font.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace odr::internal::font::cff {

/// One glyph for the CFF builder: its PostScript name and its **Type2**
/// charstring (already translated from Type1, if applicable).
struct BuilderGlyph {
  std::string name;
  std::string charstring;
};

/// Serialize a name-keyed CFF font from Type2 charstrings.
///
/// Assembles the minimal CFF a `CffFont` reader (and, after wrapping, a
/// browser) needs: Header, Name INDEX, Top DICT (FontBBox +
/// charset/CharStrings/Private offsets), String INDEX (every glyph name, SID
/// 391+), an empty Global Subr INDEX, the CharStrings INDEX, a format-0 charset
/// and a Private DICT
/// (`defaultWidthX`/`nominalWidthX`). Glyph 0 is the implicit `.notdef`; the
/// caller orders @p glyphs so glyph 0 is `.notdef`.
///
/// This is the assembly target for the Type1 -> CFF path: the translated Type2
/// charstrings go in here, the result feeds `CffFont` + `wrap_to_otf`. No
/// `FontMatrix` is emitted, so the font is 1000
/// units/em (the Type1 default); a non-default matrix is a follow-up.
///
/// Offsets in the Top DICT use the fixed-width 5-byte integer form so the
/// layout resolves in a single pass.
[[nodiscard]] std::string build_cff(std::string_view name,
                                    const std::vector<BuilderGlyph> &glyphs,
                                    double default_width, double nominal_width,
                                    FontBBox bbox);

} // namespace odr::internal::font::cff
