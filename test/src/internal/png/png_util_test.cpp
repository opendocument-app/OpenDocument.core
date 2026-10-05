#include <odr/internal/png/png_util.hpp>

#include <internal/png/png_test_util.hpp>

#include <cstdint>
#include <random>
#include <string>

#include <gtest/gtest.h>

using namespace odr::internal;
using odr::test::png::bytes;
using odr::test::png::decode_png;
using odr::test::png::DecodedPng;

TEST(PngUtil, rgb_round_trip) {
  // 2x2: red, green / blue, white
  const std::string rgb =
      bytes({255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255});

  const DecodedPng png = decode_png(png::write(rgb, 2, 2, 3));

  EXPECT_EQ(2, png.width);
  EXPECT_EQ(2, png.height);
  EXPECT_EQ(rgb, png.pixels);
}

TEST(PngUtil, a_buffer_too_short_for_the_size_is_refused) {
  EXPECT_TRUE(png::write(bytes({255, 0, 0}), 2, 2, 3).empty());
  EXPECT_TRUE(png::write("", 0, 0, 3).empty());
  EXPECT_TRUE(png::write("", 65536, 65536, 4).empty());
}

TEST(PngUtil, only_three_or_four_channels) {
  const std::string pixels(2 * 2 * 4, '\0');
  EXPECT_TRUE(png::write(pixels, 2, 2, 1).empty());
  EXPECT_TRUE(png::write(pixels, 2, 2, 2).empty());
  EXPECT_FALSE(png::write(pixels, 2, 2, 4).empty());
}

TEST(PngUtil, write_indexed_rejects_bad_input) {
  const std::string two = bytes({0, 0, 0, 255, 255, 255});
  EXPECT_FALSE(png::write_indexed(bytes({0}), 8, 1, 1, two).empty());
  EXPECT_TRUE(png::write_indexed(bytes({0}), 8, 1, 3, two).empty());
  EXPECT_TRUE(png::write_indexed(bytes({0}), 8, 2, 1, two).empty());
  EXPECT_TRUE(png::write_indexed(bytes({0}), 8, 1, 1, "").empty());
  EXPECT_TRUE(png::write_indexed("", 65536, 65536, 8, two).empty());
  EXPECT_TRUE(
      png::write_indexed(bytes({0}), 8, 1, 1, two + two.substr(0, 3)).empty());
}

TEST(PngUtil, image_data_spans_chunks_without_restarting_compression) {
  std::string rgb(256 * 256 * 3, '\0');
  std::mt19937 random(0);
  for (char &byte : rgb) {
    byte = static_cast<char>(random() & 0xff);
  }
  const DecodedPng png = decode_png(png::write(rgb, 256, 256, 3));
  EXPECT_GT(png.idat_chunks, 1);
  EXPECT_EQ(png.pixels, rgb);
}

TEST(PngUtil, rgba_round_trip) {
  // 2x1: opaque red, half-transparent green.
  const std::string rgba = bytes({255, 0, 0, 255, 0, 255, 0, 128});
  const DecodedPng png = decode_png(odr::internal::png::write(rgba, 2, 1, 4));
  EXPECT_EQ(png.width, 2);
  EXPECT_EQ(png.height, 1);
  EXPECT_EQ(png.pixels, rgba);
}
