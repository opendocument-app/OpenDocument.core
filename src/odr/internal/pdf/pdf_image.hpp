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

/// A signed 32-bit image parameter: @p fallback where it is absent, nothing
/// where it is no integer or does not fit.
std::optional<std::int32_t> image_integer(const Object &value,
                                          std::int32_t fallback);

/// Image sample depths allowed by ISO 32000-1 Table 89.
bool valid_image_bit_depth(std::int32_t bits);

/// Browser-ready image bytes and the format naming them (`image/jpeg` or
/// `image/png`).
struct EncodedImage {
  std::string data;
  std::string mime;
};

/// Convert encoded samples to JPEG or PNG; nullopt if unsupported or invalid.
/// JPEG passes through without masks; JPX ignores Decode (Table 89).
std::optional<EncodedImage>
encode_image(std::string raw, const Object &filter, const Object &decode_parms,
             std::int32_t width, std::int32_t height,
             std::int32_t bits_per_component, const ColorSpaceDef *color_space,
             std::span<const double> decode,
             std::span<const std::uint8_t> alpha = {},
             std::span<const double> color_key = {},
             std::int32_t smask_in_data = 0, const DecodeOptions &options = {});

/// Decode 1/2/4/8/16-bit samples into PNG; empty for invalid parameters.
/// Alpha is row-major coverage; color_key is [min0 max0 …] in raw sample units.
std::string encode_image_png(const std::string &samples, std::int32_t width,
                             std::int32_t height,
                             std::int32_t bits_per_component,
                             const ColorSpaceDef &color_space,
                             std::span<const double> decode,
                             std::span<const std::uint8_t> alpha = {},
                             std::span<const double> color_key = {});

/// Decode and resample a soft or 1-bit stencil mask to the base image size.
/// Stencil values of 1 mask out pixels; empty for invalid parameters.
std::vector<std::uint8_t>
decode_mask_alpha(const std::string &samples, std::int32_t width,
                  std::int32_t height, std::int32_t bits_per_component,
                  std::span<const double> decode, bool stencil,
                  std::int32_t base_width, std::int32_t base_height);

/// Paint a 1-bit stencil in the fill color: decoded 0 paints, 1 is transparent.
/// Decode [1 0] inverts coverage; empty for invalid dimensions (8.9.6.2).
std::string encode_stencil_png(const std::string &samples, std::int32_t width,
                               std::int32_t height,
                               const std::array<double, 3> &color,
                               std::span<const double> decode);

} // namespace odr::internal::pdf
