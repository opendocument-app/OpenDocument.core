#include <odr/internal/pdf/pdf_jpx.hpp>

#include <odr/internal/crypto/crypto_util.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include <gtest/gtest.h>

using namespace odr::internal::pdf;

// Bytes that are no codestream are rejected, not decoded into a raster.
TEST(PdfJpx, rejects_non_codestream) {
  EXPECT_FALSE(decode_jpx("").has_value());
  EXPECT_FALSE(decode_jpx("not a codestream").has_value());
  EXPECT_FALSE(decode_jpx(std::string(64, '\0')).has_value());
}

// A JP2 signature box with nothing behind it, and a truncated raw codestream:
// both take the header path far enough to matter.
TEST(PdfJpx, rejects_truncated_input) {
  const std::string signature(
      "\x00\x00\x00\x0c\x6a\x50\x20\x20\x0d\x0a\x87\x0a", 12);
  EXPECT_FALSE(decode_jpx(signature).has_value());
  EXPECT_FALSE(decode_jpx(std::string("\xff\x4f\xff\x51", 4)).has_value());
}

// OpenJPEG 2.5 lossless codestreams: black, half-range and white, one row.
TEST(PdfJpx, scales_samples_to_eight_bits) {
  const std::array cases{
      std::pair{
          std::uint8_t{255},
          std::string_view{
              "ff4fff5100290000000000030000000100000000000000000000000300000001"
              "00000000000000000001000101ff52000c00000001000004040001ff5c000440"
              "08ff64001000016f6472204a50582074657374ff90000a0000000000110001ff"
              "93d08007ffd9"}},
      std::pair{
          std::uint8_t{136},
          std::string_view{
              "ff4fff5100290000000000030000000100000000000000000000000300000001"
              "00000000000000000001030101ff52000c00000001000004040001ff5c000440"
              "20ff64001000016f6472204a50582074657374ff90000a0000000000140001ff"
              "93df203007245fffd9"}},
      std::pair{
          std::uint8_t{128},
          std::string_view{
              "ff4fff5100290000000000030000000100000000000000000000000300000001"
              "00000000000000000001070101ff52000c00000001000004040001ff5c000440"
              "40ff64001000016f6472204a50582074657374ff90000a0000000000150001ff"
              "93df802007246113ffd9"}},
      std::pair{
          std::uint8_t{128},
          std::string_view{
              "ff4fff5100290000000000030000000100000000000000000000000300000001"
              "000000000000000000010b0101ff52000c00000001000004040001ff5c000440"
              "60ff64001000016f6472204a50582074657374ff90000a0000000000170001ff"
              "93dfe018072461149f3fffd9"}},
  };
  for (const auto &[middle, hex] : cases) {
    SCOPED_TRACE(hex);
    const auto image = decode_jpx(odr::internal::crypto::util::hex_decode(hex));
    ASSERT_TRUE(image.has_value());
    EXPECT_EQ(image->width, 3);
    EXPECT_EQ(image->height, 1);
    EXPECT_EQ(image->components, 1);
    ASSERT_EQ(image->samples.size(), 3);
    EXPECT_EQ(static_cast<std::uint8_t>(image->samples[0]), 0);
    EXPECT_EQ(static_cast<std::uint8_t>(image->samples[1]), middle);
    EXPECT_EQ(static_cast<std::uint8_t>(image->samples[2]), 255);
  }
}
