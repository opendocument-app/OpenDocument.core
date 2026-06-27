#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace odr::internal::pdf {

class Object;
struct ColorSpaceDef;

/// Browser-ready image bytes and the format naming them (`image/jpeg` or
/// `image/png`).
struct EncodedImage {
  std::string data;
  std::string mime;
};

/// Turn an image's raw (still filter-encoded) stream bytes into browser-ready
/// bytes: a JPEG (`DCTDecode`) passes through undecoded, a fully decodable
/// raster (Flate/LZW/RunLength/ASCII/raw) is decoded and re-encoded as PNG
/// through `color_space`. `filter`/`decode_parms` are the resolved
/// `/Filter`/`/DecodeParms`; `color_space` may be null (used only by the raster
/// path — a null there yields nullopt). Returns nullopt for a codec not yet
/// handled (JPXDecode/CCITTFaxDecode/JBIG2Decode) or an inconsistent raster.
/// Shared by image XObjects and inline images.
std::optional<EncodedImage>
encode_image(std::string raw, const Object &filter, const Object &decode_parms,
             std::int32_t width, std::int32_t height,
             std::int32_t bits_per_component, const ColorSpaceDef *color_space,
             const std::vector<double> &decode);

/// Assemble a raster image's decoded samples into a base-level (8-bit, RGB)
/// PNG (ISO 32000-1 8.9.5 image samples -> a browser-ready raster). `samples`
/// holds the raw component values packed most-significant-bit first, each row
/// padded to a byte boundary. `bits_per_component` is 1/2/4/8/16;
/// `color_space` supplies the component count and the sample -> sRGB
/// conversion (Indexed palettes included). `decode`, when non-empty, is the
/// `/Decode` array ([min, max] per component) remapping the sample range.
/// Returns "" for an inconsistent configuration, so the caller skips the image.
std::string encode_image_png(const std::string &samples, std::int32_t width,
                             std::int32_t height,
                             std::int32_t bits_per_component,
                             const ColorSpaceDef &color_space,
                             const std::vector<double> &decode);

/// Wrap 8-bit RGB pixels (`3 * width * height` bytes, row-major, no padding)
/// into a PNG (single `IDAT`, no interlacing). The compression core; exposed
/// for testing the container independently of sample assembly.
std::string write_png_rgb(const std::string &rgb, std::int32_t width,
                          std::int32_t height);

} // namespace odr::internal::pdf
