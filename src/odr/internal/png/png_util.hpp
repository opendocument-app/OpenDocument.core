#pragma once

#include <cstdint>
#include <string>

namespace odr::internal::png {

/// Wraps 8-bit pixels (row-major, unpadded, top to bottom) into a png: one
/// `IDAT`, no interlacing, every scanline filtered as `None` (PNG 9.2).
/// @p channels is 3 (rgb) or 4 (rgba); anything else yields `""`.
std::string write(const std::string &pixels, std::int32_t width,
                  std::int32_t height, std::int32_t channels);

/// Wraps palette indices (MSB first, rows padded to a byte, top to bottom) into
/// an indexed-colour png (PNG 11.2.2) of @p bit_depth 1, 2, 4 or 8. @p palette
/// holds 3 bytes (rgb) an entry, at most `2^bit_depth` of them; anything else
/// yields `""`.
std::string write_indexed(const std::string &rows, std::int32_t width,
                          std::int32_t height, std::int32_t bit_depth,
                          const std::string &palette);

} // namespace odr::internal::png
