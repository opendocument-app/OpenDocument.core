#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace odr::internal::util::image {

/// Byte count for a positive raster, or nothing if it cannot fit a buffer.
inline std::optional<std::size_t> buffer_size(const std::int32_t width,
                                              const std::int32_t height,
                                              const std::size_t channels = 1) {
  if (width <= 0 || height <= 0 || channels == 0) {
    return {};
  }
  const auto pixels = static_cast<std::uint64_t>(width) * height;
  if (pixels > std::string{}.max_size() / channels) {
    return {};
  }
  return static_cast<std::size_t>(pixels) * channels;
}

} // namespace odr::internal::util::image
