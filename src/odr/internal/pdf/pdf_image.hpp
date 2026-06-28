#pragma once

#include <array>
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
/// path — a null there yields nullopt). `alpha` is an optional per-pixel
/// coverage plane (one byte per base pixel, row-major; 0 = transparent,
/// 255 = opaque) resolved from a `/SMask` or stencil `/Mask`; `color_key` is an
/// optional `/Mask` colour-key array ([min0 max0 ...] in raw sample units) —
/// when either is present the raster path emits an RGBA PNG (both are ignored
/// by the JPEG pass-through). Returns nullopt for a codec not yet handled
/// (JPXDecode/CCITTFaxDecode/JBIG2Decode) or an inconsistent raster. Shared by
/// image XObjects and inline images.
std::optional<EncodedImage>
encode_image(std::string raw, const Object &filter, const Object &decode_parms,
             std::int32_t width, std::int32_t height,
             std::int32_t bits_per_component, const ColorSpaceDef *color_space,
             const std::vector<double> &decode,
             const std::vector<std::uint8_t> &alpha = {},
             const std::vector<double> &color_key = {});

/// Assemble a raster image's decoded samples into a base-level (8-bit, RGB)
/// PNG (ISO 32000-1 8.9.5 image samples -> a browser-ready raster). `samples`
/// holds the raw component values packed most-significant-bit first, each row
/// padded to a byte boundary. `bits_per_component` is 1/2/4/8/16;
/// `color_space` supplies the component count and the sample -> sRGB
/// conversion (Indexed palettes included). `decode`, when non-empty, is the
/// `/Decode` array ([min, max] per component) remapping the sample range.
/// Returns "" for an inconsistent configuration, so the caller skips the image.
/// `alpha` (one byte per pixel, row-major) and `color_key` ([min0 max0 ...] in
/// raw sample units) add transparency: when either is given the output is an
/// RGBA PNG, a pixel made transparent where its coverage is 0 or every
/// component falls inside the colour-key ranges (ISO 32000-1 8.9.6).
std::string encode_image_png(const std::string &samples, std::int32_t width,
                             std::int32_t height,
                             std::int32_t bits_per_component,
                             const ColorSpaceDef &color_space,
                             const std::vector<double> &decode,
                             const std::vector<std::uint8_t> &alpha = {},
                             const std::vector<double> &color_key = {});

/// Resolve a `/SMask` or stencil `/Mask` sub-image (a single-component raster)
/// into a coverage plane sized to the *base* image (one byte per base pixel,
/// row-major), nearest-neighbour resampled from the mask's own `width`/`height`
/// (the two need not match — ISO 32000-1 8.9.5.4 / 11.6.5.2). `samples` is the
/// mask's filter-decoded bytes. For a soft mask (`stencil` false) the decoded
/// grey value becomes the alpha; for an explicit stencil `/Mask` (`stencil`
/// true, 1 bpc) a sample decoding to 1 is masked out (alpha 0). `decode`, when
/// non-empty, is the mask's `/Decode`. Returns empty for an inconsistent mask.
std::vector<std::uint8_t>
decode_mask_alpha(const std::string &samples, std::int32_t width,
                  std::int32_t height, std::int32_t bits_per_component,
                  const std::vector<double> &decode, bool stencil,
                  std::int32_t base_width, std::int32_t base_height);

/// Wrap 8-bit RGB pixels (`3 * width * height` bytes, row-major, no padding)
/// into a PNG (single `IDAT`, no interlacing). The compression core; exposed
/// for testing the container independently of sample assembly.
std::string write_png_rgb(const std::string &rgb, std::int32_t width,
                          std::int32_t height);

/// Wrap 8-bit RGBA pixels (`4 * width * height` bytes, row-major, no padding)
/// into a PNG (colour type 6). The transparency-carrying sibling of
/// `write_png_rgb`.
std::string write_png_rgba(const std::string &rgba, std::int32_t width,
                           std::int32_t height);

/// Paint a 1-bit stencil image mask (ISO 32000-1 8.9.6.2) in `color` (sRGB in
/// [0, 1]): a sample whose decoded value is 0 paints `color` opaquely, a 1 is
/// transparent — inverted by a `/Decode` of `[1 0]`. `samples` is the decoded
/// 1-bpc bitmap (rows byte-aligned, MSB first). The stencil's paint colour is
/// the graphics-state fill colour at draw time, so this is resolved by the page
/// extractor, not at parse time. Returns an RGBA PNG, or "" when inconsistent.
std::string encode_stencil_png(const std::string &samples, std::int32_t width,
                               std::int32_t height,
                               const std::array<double, 3> &color,
                               const std::vector<double> &decode);

} // namespace odr::internal::pdf
