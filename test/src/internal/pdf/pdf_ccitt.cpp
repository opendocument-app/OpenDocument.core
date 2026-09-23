#include <odr/internal/pdf/pdf_ccitt.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

using namespace odr::internal::pdf;

namespace {

/// The vectors below are libtiff's encoding (`tiffcp -c g4|g3:1d|g3:2d`) of
/// this picture, `#` black.
const std::vector<std::string> ring = {
    "................", //
    "....########....", //
    "...##......##...", //
    "..##..####..##..", //
    "..##..####..##..", //
    "...##......##...", //
    "....########....", //
    "#..............#", //
};

const std::string
    ring_group_4("\x9b\x16\x8f\xce\x91\x76\xdf\xfb\x62\x95\x8a\x93\x54\x2a"
                 "\x00\x20\x02",
                 17);

/// Decoded samples as rows of `#` (black) and `.`.
std::vector<std::string> picture(const std::string &samples,
                                 const std::int32_t columns,
                                 const bool black_is_1 = false) {
  const std::size_t row_bytes = (static_cast<std::size_t>(columns) + 7) / 8;
  std::vector<std::string> rows;
  for (std::size_t start = 0; start + row_bytes <= samples.size();
       start += row_bytes) {
    std::string row;
    for (std::int32_t x = 0; x < columns; ++x) {
      const auto byte = static_cast<std::uint8_t>(samples[start + x / 8]);
      const bool one = ((byte >> (7 - x % 8)) & 1) != 0;
      row += one == black_is_1 ? '#' : '.';
    }
    rows.push_back(row);
  }
  return rows;
}

CcittParameters parameters(const std::int32_t k, const std::int32_t columns,
                           const std::int32_t rows = 0) {
  CcittParameters result;
  result.k = k;
  result.columns = columns;
  result.rows = rows;
  return result;
}

} // namespace

TEST(PdfCcitt, group_4) {
  const std::optional<std::string> samples =
      decode_ccitt(ring_group_4, parameters(-1, 16));
  ASSERT_TRUE(samples.has_value());
  EXPECT_EQ(picture(*samples, 16), ring);
}

TEST(PdfCcitt, group_3_one_dimensional) {
  const std::string data(
      "\x00\x1a\x80\x06\xc5\xb0\x01\x8f\xb8\x00\x17\xdd\xbe\xe0"
      "\x02\xfb\xb7\xdc\x00\x63\xee\x00\x06\xc5\xb0\x01\x35\x5a"
      "\x20",
      29);

  const std::optional<std::string> samples =
      decode_ccitt(data, parameters(0, 16));
  ASSERT_TRUE(samples.has_value());
  EXPECT_EQ(picture(*samples, 16), ring);
}

TEST(PdfCcitt, group_3_two_dimensional) {
  const std::string data(
      "\x00\x1d\x40\x02\x36\x2c\x00\x71\xf7\x00\x02\x48\xbb\x6e"
      "\x00\x37\xdd\xbe\xe0\x02\x6c\x52\x80\x0e\xc5\xb0\x01\x13"
      "\x54\x2a",
      30);

  const std::optional<std::string> samples =
      decode_ccitt(data, parameters(1, 16));
  ASSERT_TRUE(samples.has_value());
  EXPECT_EQ(picture(*samples, 16), ring);
}

// Zero fill before each EOL, so that every row starts on a byte.
TEST(PdfCcitt, group_3_byte_aligned) {
  const std::string data(
      "\x00\x01\xa8\x00\x01\xb1\x6c\x00\x01\x8f\xb8\x00\x01\x7d"
      "\xdb\xee\x00\x01\x7d\xdb\xee\x00\x01\x8f\xb8\x00\x01\xb1"
      "\x6c\x00\x01\x35\x5a\x20",
      34);

  CcittParameters byte_aligned = parameters(0, 16);
  byte_aligned.encoded_byte_align = true;
  const std::optional<std::string> samples = decode_ccitt(data, byte_aligned);
  ASSERT_TRUE(samples.has_value());
  EXPECT_EQ(picture(*samples, 16), ring);
}

TEST(PdfCcitt, black_is_1) {
  CcittParameters inverted = parameters(-1, 16);
  inverted.black_is_1 = true;
  const std::optional<std::string> samples =
      decode_ccitt(ring_group_4, inverted);
  ASSERT_TRUE(samples.has_value());
  EXPECT_EQ(picture(*samples, 16, true), ring);
}

// `/Rows` past the data: the missing rows are white.
TEST(PdfCcitt, rows_past_the_data_are_white) {
  const std::optional<std::string> samples =
      decode_ccitt(ring_group_4, parameters(-1, 16, 10));
  ASSERT_TRUE(samples.has_value());
  std::vector<std::string> expected = ring;
  expected.resize(10, std::string(16, '.'));
  EXPECT_EQ(picture(*samples, 16), expected);
}

// `/Rows` short of the data: decoding stops there.
TEST(PdfCcitt, rows_cut_the_data) {
  const std::optional<std::string> samples =
      decode_ccitt(ring_group_4, parameters(-1, 16, 3));
  ASSERT_TRUE(samples.has_value());
  EXPECT_EQ(picture(*samples, 16),
            std::vector<std::string>(ring.begin(), ring.begin() + 3));
}

TEST(PdfCcitt, rejects_invalid_data) {
  EXPECT_FALSE(decode_ccitt("", parameters(-1, 16)).has_value());
  // `0000001` opens the uncompressed-mode extension, which is not supported
  EXPECT_FALSE(decode_ccitt("\x02\xff", parameters(-1, 16)).has_value());
  // a white run longer than the row
  EXPECT_FALSE(decode_ccitt("\xd9\x2c", parameters(0, 16)).has_value());
  EXPECT_FALSE(decode_ccitt("\x80", parameters(-1, 0)).has_value());
}
