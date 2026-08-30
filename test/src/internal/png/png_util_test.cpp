#include <odr/internal/png/png_util.hpp>

#include <odr/internal/crypto/crypto_util.hpp>

#include <cstdint>
#include <string>

#include <gtest/gtest.h>

using namespace odr::internal;

namespace {

std::string bytes(const std::initializer_list<int> values) {
  std::string result;
  for (const int value : values) {
    result.push_back(static_cast<char>(value));
  }
  return result;
}

std::uint32_t be32(const std::string &data, const std::size_t at) {
  return static_cast<std::uint8_t>(data[at]) << 24 |
         static_cast<std::uint8_t>(data[at + 1]) << 16 |
         static_cast<std::uint8_t>(data[at + 2]) << 8 |
         static_cast<std::uint8_t>(data[at + 3]);
}

struct DecodedPng final {
  std::int32_t width{};
  std::int32_t height{};
  std::string rgb;
};

/// Reads back what @ref png::write wrote: the chunks, then the one
/// zlib stream their `IDAT` holds, minus the filter byte per row.
DecodedPng decode_png(const std::string &png) {
  DecodedPng result;
  EXPECT_EQ(png.substr(1, 3), "PNG");

  std::string idat;
  std::size_t at = 8;
  while (at + 12 <= png.size()) {
    const auto length = static_cast<std::size_t>(be32(png, at));
    const std::string type = png.substr(at + 4, 4);
    const std::string data = png.substr(at + 8, length);
    if (type == "IHDR") {
      result.width = static_cast<std::int32_t>(be32(data, 0));
      result.height = static_cast<std::int32_t>(be32(data, 4));
      EXPECT_EQ(static_cast<std::uint8_t>(data[8]), 8); // bit depth
      EXPECT_EQ(static_cast<std::uint8_t>(data[9]), 2); // colour type rgb
    } else if (type == "IDAT") {
      idat += data;
    } else if (type == "IEND") {
      break;
    }
    at += 12 + length;
  }

  const std::string raw = crypto::util::zlib_inflate(idat);
  const auto stride = static_cast<std::size_t>(result.width) * 3;
  for (std::int32_t y = 0; y < result.height; ++y) {
    const std::size_t row = static_cast<std::size_t>(y) * (stride + 1);
    EXPECT_EQ(static_cast<std::uint8_t>(raw[row]), 0); // filter type none
    result.rgb.append(raw, row + 1, stride);
  }
  return result;
}

} // namespace

TEST(PngUtil, rgb_round_trip) {
  // 2x2: red, green / blue, white
  const std::string rgb =
      bytes({255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255});

  const DecodedPng png = decode_png(png::write(rgb, 2, 2, 3));

  EXPECT_EQ(2, png.width);
  EXPECT_EQ(2, png.height);
  EXPECT_EQ(rgb, png.rgb);
}

TEST(PngUtil, a_buffer_too_short_for_the_size_is_refused) {
  EXPECT_TRUE(png::write(bytes({255, 0, 0}), 2, 2, 3).empty());
  EXPECT_TRUE(png::write("", 0, 0, 3).empty());
}

TEST(PngUtil, only_three_or_four_channels) {
  const std::string pixels(2 * 2 * 4, '\0');
  EXPECT_TRUE(png::write(pixels, 2, 2, 1).empty());
  EXPECT_TRUE(png::write(pixels, 2, 2, 2).empty());
  EXPECT_FALSE(png::write(pixels, 2, 2, 4).empty());
}
