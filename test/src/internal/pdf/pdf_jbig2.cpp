#include <odr/internal/pdf/pdf_jbig2.hpp>

#include <odr/internal/util/byte_string.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>

using namespace odr::internal::pdf;

namespace {

namespace bs = odr::internal::util::byte_string;

/// One segment of an embedded-format stream (ITU-T T.88 7.2), with the short
/// header form: no referred-to segments, one-byte page association.
std::string segment(const std::uint32_t number, const std::uint8_t type,
                    const std::string &data) {
  std::string out;
  bs::put_u32_be(out, number);
  out += static_cast<char>(type);
  out += '\0';   // referred-to flags: no referred-to segments
  out += '\x01'; // page association
  bs::put_u32_be(out, static_cast<std::uint32_t>(data.size()));
  out += data;
  return out;
}

/// The page information segment's data field (7.4.8).
std::string page_info(const std::uint32_t width, const std::uint32_t height,
                      const std::uint8_t flags) {
  std::string out;
  bs::put_u32_be(out, width);
  bs::put_u32_be(out, height);
  bs::put_u32_be(out, 0); // x resolution
  bs::put_u32_be(out, 0); // y resolution
  out += static_cast<char>(flags);
  bs::put_u16_be(out, 0); // striping information
  return out;
}

} // namespace

// Nothing that could be a segment stream decodes into a page.
TEST(PdfJbig2, rejects_non_stream) {
  EXPECT_FALSE(decode_jbig2("", "").has_value());
  EXPECT_FALSE(decode_jbig2("not a jbig2 stream", "").has_value());
  EXPECT_FALSE(decode_jbig2(std::string(8, '\0'), "").has_value());
}

// A page whose every pixel keeps the default value still decodes: the header
// walk, the page allocation and the packing are the whole path.
TEST(PdfJbig2, decodes_a_blank_page) {
  const std::string stream = segment(0, 48, page_info(16, 2, 0x00));

  const std::optional<Jbig2Image> image = decode_jbig2(stream, "");
  ASSERT_TRUE(image.has_value());
  EXPECT_EQ(image->width, 16);
  EXPECT_EQ(image->height, 2);
  // JBIG2's 0 is white, and the samples are inverted to the `/DeviceGray`
  // sense the filter delivers: white is 1.
  EXPECT_EQ(image->samples, std::string(4, '\xff'));
}

// The default pixel value seeds the page, and inverts on the way out.
TEST(PdfJbig2, honors_the_default_pixel_value) {
  const std::string stream = segment(0, 48, page_info(16, 2, 0x04));

  const std::optional<Jbig2Image> image = decode_jbig2(stream, "");
  ASSERT_TRUE(image.has_value());
  EXPECT_EQ(image->samples, std::string(4, '\0'));
}

// Rows are padded to a byte boundary (ISO 32000-1 8.9.5.2), so a 12-pixel row
// occupies two bytes and the four spare bits read as white.
TEST(PdfJbig2, pads_rows_to_a_byte) {
  const std::string stream = segment(0, 48, page_info(12, 3, 0x00));

  const std::optional<Jbig2Image> image = decode_jbig2(stream, "");
  ASSERT_TRUE(image.has_value());
  EXPECT_EQ(image->samples.size(), 6);
}

// 7.2.4's long form: a referred-to count of 7 escapes to a four-byte count
// followed by one retain bit per referred-to segment. The walk has to step
// over both to find the next segment.
TEST(PdfJbig2, walks_the_long_referred_to_header_form) {
  std::string stream;
  bs::put_u32_be(stream, 0);
  stream += static_cast<char>(48);    // page information
  bs::put_u32_be(stream, 0xe0000000); // count escape, zero referred-to
  stream += '\0';                     // retain bits
  stream += '\x01';                   // page association
  const std::string data = page_info(8, 1, 0x00);
  bs::put_u32_be(stream, static_cast<std::uint32_t>(data.size()));
  stream += data;

  const std::optional<Jbig2Image> image = decode_jbig2(stream, "");
  ASSERT_TRUE(image.has_value());
  EXPECT_EQ(image->width, 8);
  EXPECT_EQ(image->height, 1);
}

// A four-byte page association is flagged in the segment header, and skipping
// the wrong number of bytes would desynchronise the walk.
TEST(PdfJbig2, walks_a_four_byte_page_association) {
  std::string stream;
  bs::put_u32_be(stream, 0);
  stream += static_cast<char>(48 | 0x40); // page association is four bytes
  stream += '\0';                         // no referred-to segments
  bs::put_u32_be(stream, 1);              // page association
  const std::string data = page_info(8, 1, 0x00);
  bs::put_u32_be(stream, static_cast<std::uint32_t>(data.size()));
  stream += data;

  const std::optional<Jbig2Image> image = decode_jbig2(stream, "");
  ASSERT_TRUE(image.has_value());
  EXPECT_EQ(image->width, 8);
}

// A segment claiming more bytes than the stream holds is refused rather than
// read past.
TEST(PdfJbig2, rejects_a_segment_past_the_end) {
  std::string stream = segment(0, 48, page_info(16, 2, 0x00));
  bs::put_u32_be(stream, 1);
  stream += static_cast<char>(38); // immediate generic region
  stream += '\0';
  stream += '\x01';
  bs::put_u32_be(stream, 4096); // more data than follows
  stream += "short";

  EXPECT_FALSE(decode_jbig2(stream, "").has_value());
}

// A feature we do not decode fails the image rather than painting a page that
// is missing part of its content.
TEST(PdfJbig2, rejects_an_unsupported_segment) {
  std::string stream = segment(0, 48, page_info(16, 2, 0x00));
  stream += segment(1, 16, std::string(4, '\0')); // pattern dictionary

  EXPECT_FALSE(decode_jbig2(stream, "").has_value());
}

// A region arriving before the page it composes onto is a broken stream.
TEST(PdfJbig2, rejects_a_region_without_a_page) {
  std::string region;
  bs::put_u32_be(region, 8); // width
  bs::put_u32_be(region, 8); // height
  bs::put_u32_be(region, 0); // x
  bs::put_u32_be(region, 0); // y
  region += '\0';            // external combination operator OR
  region += '\0';            // generic region flags: arithmetic, template 0
  region += std::string(8, '\0'); // the four AT pixels
  region += std::string(16, '\0');

  EXPECT_FALSE(decode_jbig2(segment(0, 38, region), "").has_value());
}

// A symbol dictionary that resumes a referred dictionary's coding contexts
// would decode from the wrong probability states, so it is refused.
TEST(PdfJbig2, rejects_a_retained_coding_context) {
  std::string dictionary;
  bs::put_u16_be(dictionary, 0x0100); // bitmap coding context used
  dictionary += std::string(8, '\0'); // the four AT pixels
  bs::put_u32_be(dictionary, 1);      // exported symbols
  bs::put_u32_be(dictionary, 1);      // new symbols
  dictionary += std::string(16, '\0');

  std::string stream = segment(0, 48, page_info(8, 8, 0x00));
  stream += segment(1, 0, dictionary);

  EXPECT_FALSE(decode_jbig2(stream, "").has_value());
}

// An MMR-coded generic region needs the CCITT decoder we do not have.
TEST(PdfJbig2, rejects_mmr_coding) {
  std::string region;
  bs::put_u32_be(region, 8);
  bs::put_u32_be(region, 8);
  bs::put_u32_be(region, 0);
  bs::put_u32_be(region, 0);
  region += '\0';
  region += '\x01'; // MMR
  region += std::string(16, '\0');

  std::string stream = segment(0, 48, page_info(8, 8, 0x00));
  stream += segment(1, 38, region);

  EXPECT_FALSE(decode_jbig2(stream, "").has_value());
}
