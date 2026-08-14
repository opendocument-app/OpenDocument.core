#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace odr::internal::pdf {

/// A JPEG 2000 codestream decoded to 8-bit interleaved samples.
struct JpxImage {
  std::int32_t width{0};
  std::int32_t height{0};
  std::int32_t components{0};      ///< colour components, alpha excluded
  std::string samples;             ///< `components` bytes per pixel, row-major
  std::vector<std::uint8_t> alpha; ///< empty unless the image carries one
};

/// Decode a `JPXDecode` payload — a JP2 container or a bare J2K codestream
/// (ISO 32000-1 7.4.9). `nullopt` for a codestream openjpeg rejects or a
/// layout we do not map (more than four colour components).
std::optional<JpxImage> decode_jpx(const std::string &data);

} // namespace odr::internal::pdf
