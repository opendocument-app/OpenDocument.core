#pragma once

#include <cstdint>
#include <string>

namespace odr::internal::util::png {

/// Wraps 8-bit pixels (row-major, unpadded, top to bottom) into a png: one
/// `IDAT`, no interlacing, every scanline filtered as `None` (PNG 9.2).
/// @p channels is 3 (rgb) or 4 (rgba); anything else yields `""`.
std::string write(const std::string &pixels, std::int32_t width,
                  std::int32_t height, std::int32_t channels);

} // namespace odr::internal::util::png
