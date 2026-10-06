#include <odr/internal/pdf/pdf_image.hpp>

#include <internal/png/png_test_util.hpp>
#include <odr/internal/pdf/pdf_color.hpp>

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace odr::internal::pdf;
using odr::test::png::bytes;
using odr::test::png::decode_png;
using odr::test::png::DecodedPng;

namespace {

ColorSpaceDef device_rgb() {
  ColorSpaceDef def;
  def.kind = ColorSpaceKind::device_rgb;
  def.components = 3;
  return def;
}

ColorSpaceDef device_gray() {
  ColorSpaceDef def;
  def.kind = ColorSpaceKind::device_gray;
  def.components = 1;
  return def;
}

} // namespace

TEST(PdfImage, encode_rgb_8bpc) {
  const std::string samples =
      bytes({10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120});
  const DecodedPng png =
      decode_png(encode_image_png(samples, 2, 2, 8, device_rgb(), {}));
  EXPECT_EQ(png.pixels, samples); // identity for DeviceRGB 8bpc
}

TEST(PdfImage, encode_gray_8bpc_expands_to_rgb) {
  const std::string samples = bytes({0, 128, 200, 255});
  const DecodedPng png =
      decode_png(encode_image_png(samples, 2, 2, 8, device_gray(), {}));
  EXPECT_EQ(png.pixel(0, 0), bytes({0, 0, 0}));
  EXPECT_EQ(png.pixel(1, 0), bytes({128, 128, 128}));
  EXPECT_EQ(png.pixel(0, 1), bytes({200, 200, 200}));
  EXPECT_EQ(png.pixel(1, 1), bytes({255, 255, 255}));
}

TEST(PdfImage, encode_indexed_2x2) {
  // Palette of two DeviceRGB entries: index 0 -> red, index 1 -> green.
  ColorSpaceDef indexed;
  indexed.kind = ColorSpaceKind::indexed;
  indexed.components = 1;
  indexed.base = std::make_shared<ColorSpaceDef>(device_rgb());
  indexed.hival = 1;
  indexed.lookup = bytes({255, 0, 0, 0, 255, 0});

  // 2x2 at 8 bpc: indices 0,1 / 1,0.
  const std::string samples = bytes({0, 1, 1, 0});
  const DecodedPng png =
      decode_png(encode_image_png(samples, 2, 2, 8, indexed, {}));
  EXPECT_EQ(png.pixel(0, 0), bytes({255, 0, 0}));
  EXPECT_EQ(png.pixel(1, 0), bytes({0, 255, 0}));
  EXPECT_EQ(png.pixel(0, 1), bytes({0, 255, 0}));
  EXPECT_EQ(png.pixel(1, 1), bytes({255, 0, 0}));
}

TEST(PdfImage, encode_indexed_1bpc_packs_and_pads_rows) {
  // 1 bpc, 3x1: indices 1,0,1 -> 0b101 in the top three bits, row padded to a
  // byte. Palette: 0 -> black, 1 -> white.
  ColorSpaceDef indexed;
  indexed.kind = ColorSpaceKind::indexed;
  indexed.components = 1;
  indexed.base = std::make_shared<ColorSpaceDef>(device_rgb());
  indexed.hival = 1;
  indexed.lookup = bytes({0, 0, 0, 255, 255, 255});

  const std::string samples = bytes({0b10100000});
  const DecodedPng png =
      decode_png(encode_image_png(samples, 3, 1, 1, indexed, {}));
  EXPECT_EQ(png.pixel(0, 0), bytes({255, 255, 255}));
  EXPECT_EQ(png.pixel(1, 0), bytes({0, 0, 0}));
  EXPECT_EQ(png.pixel(2, 0), bytes({255, 255, 255}));
  EXPECT_EQ(png.bit_depth, 1);
}

// A bilevel scan stays at a bit a pixel, its two colours from the /Decode.
TEST(PdfImage, encode_gray_1bpc_as_a_palette) {
  const std::string samples = bytes({0b01000000, 0b10000000});
  const std::array<double, 2> decode = {1.0, 0.0};
  const DecodedPng png =
      decode_png(encode_image_png(samples, 2, 2, 1, device_gray(), decode));
  EXPECT_EQ(png.colour_type, 3);
  EXPECT_EQ(png.bit_depth, 1);
  EXPECT_EQ(png.pixel(0, 0), bytes({255, 255, 255}));
  EXPECT_EQ(png.pixel(1, 0), bytes({0, 0, 0}));
  EXPECT_EQ(png.pixel(0, 1), bytes({0, 0, 0}));
  EXPECT_EQ(png.pixel(1, 1), bytes({255, 255, 255}));
}

// Rows the samples do not reach read as zero, as on the 8-bit path.
TEST(PdfImage, encode_gray_1bpc_pads_short_samples) {
  const DecodedPng png =
      decode_png(encode_image_png(bytes({0xff}), 8, 2, 1, device_gray(), {}));
  EXPECT_EQ(png.pixel(0, 0), bytes({255, 255, 255}));
  EXPECT_EQ(png.pixel(0, 1), bytes({0, 0, 0}));
}

TEST(PdfImage, encode_gray_4bpc) {
  // 4 bpc, 2x1: 0x0 and 0xF -> black and white (one byte holds both samples).
  const std::string samples = bytes({0x0F});
  const DecodedPng png =
      decode_png(encode_image_png(samples, 2, 1, 4, device_gray(), {}));
  EXPECT_EQ(png.pixel(0, 0), bytes({0, 0, 0}));
  EXPECT_EQ(png.pixel(1, 0), bytes({255, 255, 255}));
}

TEST(PdfImage, encode_honours_decode_array) {
  // DeviceGray with /Decode [1 0] inverts: sample 0 -> white, 255 -> black.
  const std::string samples = bytes({0, 255});
  const std::array<double, 2> decode = {1.0, 0.0};
  const DecodedPng png =
      decode_png(encode_image_png(samples, 2, 1, 8, device_gray(), decode));
  EXPECT_EQ(png.pixel(0, 0), bytes({255, 255, 255}));
  EXPECT_EQ(png.pixel(1, 0), bytes({0, 0, 0}));
}

TEST(PdfImage, encode_rejects_bad_parameters) {
  EXPECT_TRUE(encode_image_png("", 0, 1, 8, device_rgb(), {}).empty());
  EXPECT_TRUE(encode_image_png("", 1, 1, 0, device_rgb(), {}).empty());
  ColorSpaceDef zero;
  zero.components = 0;
  EXPECT_TRUE(encode_image_png("", 1, 1, 8, zero, {}).empty());
}

TEST(PdfImage, encode_with_alpha_plane_emits_rgba) {
  // DeviceGray 2x1, samples black/white, alpha plane opaque/transparent.
  const std::string samples = bytes({0, 255});
  const std::vector<std::uint8_t> alpha = {255, 0};
  const DecodedPng png =
      decode_png(encode_image_png(samples, 2, 1, 8, device_gray(), {}, alpha));
  EXPECT_EQ(png.pixel(0, 0), bytes({0, 0, 0, 255}));
  EXPECT_EQ(png.pixel(1, 0), bytes({255, 255, 255, 0}));
}

TEST(PdfImage, encode_with_colour_key_masks_matching_pixels) {
  // DeviceRGB 2x1: pure red is keyed out, the other pixel stays opaque.
  const std::string samples = bytes({255, 0, 0, 10, 20, 30});
  const std::vector<double> color_key = {255, 255, 0, 0, 0, 0};
  const DecodedPng png = decode_png(
      encode_image_png(samples, 2, 1, 8, device_rgb(), {}, {}, color_key));
  EXPECT_EQ(png.pixel(0, 0), bytes({255, 0, 0, 0}));
  EXPECT_EQ(png.pixel(1, 0), bytes({10, 20, 30, 255}));
}

TEST(PdfImage, encode_stencil_paints_fill_colour_through_mask) {
  // 1 bpc, 2x1: bits 0,1 -> 0b01000000 (row padded to a byte). Default /Decode
  // [0 1]: a 0 paints the fill colour, a 1 is transparent.
  const std::string samples = bytes({0b01000000});
  const DecodedPng png =
      decode_png(encode_stencil_png(samples, 2, 1, {1.0, 0.0, 0.0}, {}));
  EXPECT_EQ(png.pixel(0, 0), bytes({255, 0, 0, 255}));
  EXPECT_EQ(static_cast<std::uint8_t>(png.pixel(1, 0)[3]), 0);
}

TEST(PdfImage, encode_stencil_decode_inverts) {
  // /Decode [1 0] swaps which sample paints: now the 1 paints, the 0 is clear.
  const std::string samples = bytes({0b01000000});
  const std::array<double, 2> decode = {1.0, 0.0};
  const DecodedPng png =
      decode_png(encode_stencil_png(samples, 2, 1, {0.0, 0.0, 1.0}, decode));
  EXPECT_EQ(static_cast<std::uint8_t>(png.pixel(0, 0)[3]), 0);
  EXPECT_EQ(png.pixel(1, 0), bytes({0, 0, 255, 255}));
}

TEST(PdfImage, decode_mask_alpha_soft_mask_grey_to_alpha) {
  // 8-bpc DeviceGray soft mask, same size as the base: grey level is alpha.
  const std::string samples = bytes({0, 128, 255, 64});
  const std::vector<std::uint8_t> alpha =
      decode_mask_alpha(samples, 2, 2, 8, {}, /*stencil=*/false, 2, 2);
  EXPECT_EQ(alpha, (std::vector<std::uint8_t>{0, 128, 255, 64}));
  struct Case {
    std::array<double, 2> decode;
    std::vector<std::uint8_t> expected;
  };
  for (const Case &test :
       {Case{{0.2, 0.8}, {51, 128, 204, 89}},
        Case{{0.8, 0.2}, {204, 127, 51, 166}},
        Case{{0.5, 0.5}, {128, 128, 128, 128}}, Case{{-1, 2}, {0, 129, 255, 0}},
        Case{{std::numeric_limits<double>::quiet_NaN(), 1}, {0, 128, 255, 64}},
        Case{{0, std::numeric_limits<double>::infinity()},
             {0, 128, 255, 64}}}) {
    EXPECT_EQ(decode_mask_alpha(samples, 2, 2, 8, test.decode, false, 2, 2),
              test.expected);
  }
}

TEST(PdfImage, decode_mask_alpha_stencil_masks_set_bits) {
  // 1-bpc stencil /Mask, 2x1: a decoded 1 masks the base pixel out (alpha 0).
  const std::string samples = bytes({0b01000000});
  const std::vector<std::uint8_t> alpha =
      decode_mask_alpha(samples, 2, 1, 1, {}, /*stencil=*/true, 2, 1);
  EXPECT_EQ(alpha, (std::vector<std::uint8_t>{255, 0}));
}

TEST(PdfImage, decode_mask_alpha_resamples_to_base) {
  // A 1x1 mask scaled up to 2x2: nearest-neighbour fills every base pixel.
  const std::string samples = bytes({200});
  const std::vector<std::uint8_t> alpha =
      decode_mask_alpha(samples, 1, 1, 8, {}, /*stencil=*/false, 2, 2);
  EXPECT_EQ(alpha, (std::vector<std::uint8_t>{200, 200, 200, 200}));
}

TEST(PdfImage, resampling_large_masks_keeps_both_axes_in_range) {
  std::string samples(60000, '\0');
  samples[59998] = static_cast<char>(255);
  for (const bool vertical : {false, true}) {
    const auto alpha = decode_mask_alpha(
        samples, vertical ? 1 : 60000, vertical ? 60000 : 1, 8, {}, false,
        vertical ? 1 : 60001, vertical ? 60001 : 1);
    ASSERT_EQ(alpha.size(), 60001);
    EXPECT_EQ(alpha[59999], 255);
    EXPECT_EQ(alpha.back(), 0);
  }
}

TEST(PdfImage, rejects_unrepresentable_output_sizes) {
  constexpr auto extent = std::numeric_limits<std::int32_t>::max();
  EXPECT_TRUE(
      encode_image_png("", extent, extent, 8, device_rgb(), {}).empty());
  EXPECT_TRUE(encode_stencil_png("", extent, extent, {0, 0, 0}, {}).empty());
  if constexpr (sizeof(std::size_t) == 4) {
    EXPECT_TRUE(
        decode_mask_alpha("", 65536, 65536, 8, {}, false, 1, 1).empty());
    EXPECT_TRUE(
        decode_mask_alpha("", 1, 1, 8, {}, false, 65536, 65536).empty());
  }
}
