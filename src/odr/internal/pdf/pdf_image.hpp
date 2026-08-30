#pragma once

#include <odr/internal/pdf/pdf_filter.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
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

/// Turn an image's raw (still filter-encoded) bytes into browser-ready ones: a
/// `DCTDecode` JPEG passes through, any other decodable raster is re-encoded as
/// PNG through `color_space` (required only on that path). `alpha` and
/// `color_key` (see `encode_image_png`) make the raster RGBA and are ignored by
/// the JPEG pass-through. A `JPXDecode` raster comes through `decode_jpx`, its
/// own opacity channel taken only as `smask_in_data` says (Table 89: 0 ignores
/// it, 2 says the colour is premultiplied by it). A `JBIG2Decode` raster goes
/// through `decode_jbig2`, `options` carrying the globals it may need.
/// `nullopt` for an undecodable codec (CCITTFax, or JBIG2 past the decoder's
/// reach) or an inconsistent raster.
std::optional<EncodedImage>
encode_image(std::string raw, const Object &filter, const Object &decode_parms,
             std::int32_t width, std::int32_t height,
             std::int32_t bits_per_component, const ColorSpaceDef *color_space,
             std::span<const double> decode,
             std::span<const std::uint8_t> alpha = {},
             std::span<const double> color_key = {},
             std::int32_t smask_in_data = 0, const DecodeOptions &options = {});

/// Assemble decoded image samples (ISO 32000-1 8.9.5: MSB-first, rows padded
/// to a byte boundary, `bits_per_component` of 1/2/4/8/16) into an 8-bit PNG,
/// converting through `color_space`. `decode` is the `/Decode` array remapping
/// the sample range. `alpha` (one byte per pixel, row-major) and `color_key`
/// ([min0 max0 …] in raw sample units) each make the output RGBA, transparent
/// where coverage is 0 / every component is inside the ranges (8.9.6). Returns
/// "" for an inconsistent configuration, so the caller skips the image.
std::string encode_image_png(const std::string &samples, std::int32_t width,
                             std::int32_t height,
                             std::int32_t bits_per_component,
                             const ColorSpaceDef &color_space,
                             std::span<const double> decode,
                             std::span<const std::uint8_t> alpha = {},
                             std::span<const double> color_key = {});

/// Resolve a `/SMask` or stencil `/Mask` sub-image into a coverage plane sized
/// to the *base* image, nearest-neighbour resampled — the two resolutions need
/// not match (ISO 32000-1 8.9.5.4 / 11.6.5.2). A soft mask's grey value is the
/// alpha; a stencil (`stencil`, 1 bpc) masks out where a sample decodes to 1.
/// Empty for an inconsistent mask.
std::vector<std::uint8_t>
decode_mask_alpha(const std::string &samples, std::int32_t width,
                  std::int32_t height, std::int32_t bits_per_component,
                  std::span<const double> decode, bool stencil,
                  std::int32_t base_width, std::int32_t base_height);

/// Paint a 1-bpc stencil mask (ISO 32000-1 8.9.6.2) into an RGBA PNG: a sample
/// decoding to 0 paints `color` (sRGB in [0, 1]) opaquely, a 1 is transparent,
/// and a `/Decode` of `[1 0]` swaps that. `color` is the fill colour at draw
/// time, so only the page extractor can call this. "" when inconsistent.
std::string encode_stencil_png(const std::string &samples, std::int32_t width,
                               std::int32_t height,
                               const std::array<double, 3> &color,
                               std::span<const double> decode);

} // namespace odr::internal::pdf
