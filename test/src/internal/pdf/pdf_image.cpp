#include <odr/internal/pdf/pdf_image.hpp>

#include <odr/internal/crypto/crypto_util.hpp>
#include <odr/internal/pdf/pdf_color.hpp>

#include <cstdint>
#include <string>

#include <gtest/gtest.h>

using namespace odr::internal::pdf;

namespace {

std::uint32_t be32(const std::string &data, const std::size_t offset) {
  return (static_cast<std::uint32_t>(static_cast<std::uint8_t>(data[offset]))
          << 24) |
         (static_cast<std::uint32_t>(
              static_cast<std::uint8_t>(data[offset + 1]))
          << 16) |
         (static_cast<std::uint32_t>(
              static_cast<std::uint8_t>(data[offset + 2]))
          << 8) |
         static_cast<std::uint32_t>(
             static_cast<std::uint8_t>(data[offset + 3]));
}

/// Minimal PNG reader for the encoder's output: walks the chunks, inflates the
/// concatenated IDAT and strips the per-row filter byte (the encoder only emits
/// filter type 0), yielding the raw 8-bit RGB pixels.
struct DecodedPng {
  std::int32_t width{0};
  std::int32_t height{0};
  std::string rgb;
};

DecodedPng decode_png(const std::string &png) {
  EXPECT_GE(png.size(), 8u);
  const std::string signature = {
      static_cast<char>(0x89), 'P', 'N', 'G', '\r', '\n',
      static_cast<char>(0x1A), '\n'};
  EXPECT_EQ(png.substr(0, 8), signature);

  DecodedPng result;
  std::string idat;
  std::size_t p = 8;
  while (p + 12 <= png.size()) {
    const std::uint32_t length = be32(png, p);
    const std::string type = png.substr(p + 4, 4);
    const std::string data = png.substr(p + 8, length);
    if (type == "IHDR") {
      result.width = static_cast<std::int32_t>(be32(data, 0));
      result.height = static_cast<std::int32_t>(be32(data, 4));
      EXPECT_EQ(static_cast<std::uint8_t>(data[8]), 8); // bit depth
      EXPECT_EQ(static_cast<std::uint8_t>(data[9]), 2); // colour type RGB
    } else if (type == "IDAT") {
      idat += data;
    } else if (type == "IEND") {
      break;
    }
    p += 12 + length;
  }

  const std::string raw = odr::internal::crypto::util::zlib_inflate(idat);
  const auto stride = static_cast<std::size_t>(result.width) * 3;
  for (std::int32_t y = 0; y < result.height; ++y) {
    const std::size_t row = static_cast<std::size_t>(y) * (stride + 1);
    EXPECT_EQ(static_cast<std::uint8_t>(raw[row]), 0); // filter type None
    result.rgb.append(raw, row + 1, stride);
  }
  return result;
}

std::string rgb_pixel(const std::string &rgb, const std::int32_t width,
                      const std::int32_t x, const std::int32_t y) {
  return rgb.substr((static_cast<std::size_t>(y) * width + x) * 3, 3);
}

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

std::string bytes(std::initializer_list<int> values) {
  std::string result;
  for (const int v : values) {
    result.push_back(static_cast<char>(v));
  }
  return result;
}

} // namespace

TEST(PdfImage, write_png_rgb_round_trip) {
  // 2x2: red, green / blue, white.
  const std::string rgb =
      bytes({255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255});
  const DecodedPng png = decode_png(write_png_rgb(rgb, 2, 2));
  EXPECT_EQ(png.width, 2);
  EXPECT_EQ(png.height, 2);
  EXPECT_EQ(png.rgb, rgb);
}

TEST(PdfImage, write_png_rgb_rejects_short_buffer) {
  EXPECT_TRUE(write_png_rgb(bytes({255, 0, 0}), 2, 2).empty());
  EXPECT_TRUE(write_png_rgb("", 0, 0).empty());
}

TEST(PdfImage, encode_rgb_8bpc) {
  const std::string samples =
      bytes({10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120});
  const DecodedPng png =
      decode_png(encode_image_png(samples, 2, 2, 8, device_rgb(), {}));
  EXPECT_EQ(png.rgb, samples); // identity for DeviceRGB 8bpc
}

TEST(PdfImage, encode_gray_8bpc_expands_to_rgb) {
  const std::string samples = bytes({0, 128, 200, 255});
  const DecodedPng png =
      decode_png(encode_image_png(samples, 2, 2, 8, device_gray(), {}));
  EXPECT_EQ(rgb_pixel(png.rgb, 2, 0, 0), bytes({0, 0, 0}));
  EXPECT_EQ(rgb_pixel(png.rgb, 2, 1, 0), bytes({128, 128, 128}));
  EXPECT_EQ(rgb_pixel(png.rgb, 2, 0, 1), bytes({200, 200, 200}));
  EXPECT_EQ(rgb_pixel(png.rgb, 2, 1, 1), bytes({255, 255, 255}));
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
  EXPECT_EQ(rgb_pixel(png.rgb, 2, 0, 0), bytes({255, 0, 0}));
  EXPECT_EQ(rgb_pixel(png.rgb, 2, 1, 0), bytes({0, 255, 0}));
  EXPECT_EQ(rgb_pixel(png.rgb, 2, 0, 1), bytes({0, 255, 0}));
  EXPECT_EQ(rgb_pixel(png.rgb, 2, 1, 1), bytes({255, 0, 0}));
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
  EXPECT_EQ(rgb_pixel(png.rgb, 3, 0, 0), bytes({255, 255, 255}));
  EXPECT_EQ(rgb_pixel(png.rgb, 3, 1, 0), bytes({0, 0, 0}));
  EXPECT_EQ(rgb_pixel(png.rgb, 3, 2, 0), bytes({255, 255, 255}));
}

TEST(PdfImage, encode_gray_4bpc) {
  // 4 bpc, 2x1: 0x0 and 0xF -> black and white (one byte holds both samples).
  const std::string samples = bytes({0x0F});
  const DecodedPng png =
      decode_png(encode_image_png(samples, 2, 1, 4, device_gray(), {}));
  EXPECT_EQ(rgb_pixel(png.rgb, 2, 0, 0), bytes({0, 0, 0}));
  EXPECT_EQ(rgb_pixel(png.rgb, 2, 1, 0), bytes({255, 255, 255}));
}

TEST(PdfImage, encode_honours_decode_array) {
  // DeviceGray with /Decode [1 0] inverts: sample 0 -> white, 255 -> black.
  const std::string samples = bytes({0, 255});
  const DecodedPng png =
      decode_png(encode_image_png(samples, 2, 1, 8, device_gray(), {1.0, 0.0}));
  EXPECT_EQ(rgb_pixel(png.rgb, 2, 0, 0), bytes({255, 255, 255}));
  EXPECT_EQ(rgb_pixel(png.rgb, 2, 1, 0), bytes({0, 0, 0}));
}

TEST(PdfImage, encode_rejects_bad_parameters) {
  EXPECT_TRUE(encode_image_png("", 0, 1, 8, device_rgb(), {}).empty());
  EXPECT_TRUE(encode_image_png("", 1, 1, 0, device_rgb(), {}).empty());
  ColorSpaceDef zero;
  zero.components = 0;
  EXPECT_TRUE(encode_image_png("", 1, 1, 8, zero, {}).empty());
}
