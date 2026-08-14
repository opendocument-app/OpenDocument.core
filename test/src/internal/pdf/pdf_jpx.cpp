#include <odr/internal/pdf/pdf_jpx.hpp>

#include <string>

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
