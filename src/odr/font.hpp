#pragma once

#include <cstdint>

namespace odr {

/// @brief Source flavor of a font program.
enum class FontFormat {
  unknown,
  truetype,     ///< SFNT with `glyf` outlines (incl. CIDFontType2)
  opentype_cff, ///< SFNT wrapping a `CFF ` table (`OTTO`)
  cff,          ///< bare CFF / Type1C
  type1,        ///< Type1 / `eexec`
};

/// @brief Glyph-space bounding box, in font design units.
struct FontBBox {
  std::int16_t x_min{};
  std::int16_t y_min{};
  std::int16_t x_max{};
  std::int16_t y_max{};
};

} // namespace odr
