#pragma once

#include <odr/font.hpp>

#include <span>
#include <string>
#include <string_view>

namespace odr::internal::font::cff {

/// One glyph for the CFF builder: its PostScript name and its **Type2**
/// charstring (already translated from Type1, if applicable).
struct BuilderGlyph {
  std::string name;
  std::string charstring;
};

/// Serialize Type2 glyphs into a name-keyed CFF at 1000 units/em.
/// Glyph 0 must be .notdef; names must be unique and fit the CFF SID space.
[[nodiscard]] std::string build_cff(std::string_view name,
                                    std::span<const BuilderGlyph> glyphs,
                                    double default_width, double nominal_width,
                                    FontBBox bbox);

} // namespace odr::internal::font::cff
