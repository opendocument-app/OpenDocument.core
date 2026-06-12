#include <odr/internal/pdf/pdf_filter.hpp>
#include <odr/internal/pdf/pdf_object.hpp>

#include <stdexcept>
#include <string>

#include <cryptopp/filters.h>
#include <cryptopp/zlib.h>

#include <gtest/gtest.h>

using namespace odr::internal::pdf;

namespace {

std::string zlib_deflate(const std::string &in) {
  std::string out;
  CryptoPP::ZlibCompressor compressor(new CryptoPP::StringSink(out));
  compressor.Put(reinterpret_cast<const CryptoPP::byte *>(in.data()),
                 in.size());
  compressor.MessageEnd();
  return out;
}

Object name(const std::string &string) { return Object(Name(string)); }

Object dictionary(Dictionary::Holder holder) {
  return Object(Dictionary(std::move(holder)));
}

Object array(Array::Holder holder) { return Object(Array(std::move(holder))); }

} // namespace

TEST(PdfFilter, ascii_hex_decode) {
  EXPECT_EQ(ascii_hex_decode("48656C6C6F>"), "Hello");
  EXPECT_EQ(ascii_hex_decode("48 65\n6c6C\t6F >"), "Hello");
  // odd digit count: as if a 0 followed (0x70 = 'p')
  EXPECT_EQ(ascii_hex_decode("48656C6C6F7>"), "Hellop");
  EXPECT_EQ(ascii_hex_decode(">"), "");
  EXPECT_THROW(ascii_hex_decode("4G>"), std::runtime_error);
}

TEST(PdfFilter, ascii85_decode) {
  EXPECT_EQ(ascii85_decode("9jqo^~>"), "Man ");
  EXPECT_EQ(ascii85_decode("9j qo\n^~>"), "Man ");
  // partial group: 2 bytes from 3 characters
  EXPECT_EQ(ascii85_decode("9jn~>"), "Ma");
  // z is shorthand for four zero bytes
  EXPECT_EQ(ascii85_decode("z~>"), std::string(4, '\0'));
  // group value above 2^32 - 1
  EXPECT_THROW(ascii85_decode("uuuuu~>"), std::runtime_error);
  // single leftover character is impossible
  EXPECT_THROW(ascii85_decode("9~>"), std::runtime_error);
  EXPECT_THROW(ascii85_decode("9jzo^~>"), std::runtime_error);
}

TEST(PdfFilter, lzw_decode) {
  // ISO 32000-1 7.4.4.2, Examples 1+2: codes for 45 45 45 45 45 65 45 45 45 66
  const std::string encoded = "\x80\x0B\x60\x50\x22\x0C\x0C\x85\x01";
  EXPECT_EQ(lzw_decode(encoded), "-----A---B");
}

TEST(PdfFilter, run_length_decode) {
  // copy 3 literal bytes, then repeat 'z' 257-254 = 3 times
  EXPECT_EQ(run_length_decode("\x02"
                              "abc\xFEz\x80"),
            "abczzz");
  EXPECT_EQ(run_length_decode("\x80"), "");
  EXPECT_THROW(run_length_decode("\x05"
                                 "ab"),
               std::runtime_error);
}

TEST(PdfFilter, png_predictor_up) {
  // two rows of four bytes, both tagged Up (2)
  const std::string data("\x02\x01\x01\x01\x01"
                         "\x02\x01\x01\x01\x01",
                         10);
  EXPECT_EQ(apply_predictor(data, 12, 1, 8, 4),
            std::string("\x01\x01\x01\x01\x02\x02\x02\x02", 8));
}

TEST(PdfFilter, png_predictor_sub) {
  const std::string data("\x01\x01\x01\x01\x01", 5);
  EXPECT_EQ(apply_predictor(data, 11, 1, 8, 4),
            std::string("\x01\x02\x03\x04", 4));
}

TEST(PdfFilter, png_predictor_paeth) {
  // row 1 unfiltered (None), row 2 Paeth against row 1
  const std::string data("\x00\x0A\x14\x1E\x28"
                         "\x04\x05\x05\x05\x05",
                         10);
  EXPECT_EQ(apply_predictor(data, 14, 1, 8, 4),
            std::string("\x0A\x14\x1E\x28\x0F\x19\x23\x2D", 8));
}

TEST(PdfFilter, png_predictor_rejects_partial_row) {
  EXPECT_THROW(apply_predictor(std::string("\x02\x01", 2), 12, 1, 8, 4),
               std::runtime_error);
}

TEST(PdfFilter, tiff_predictor) {
  // one row, three colour components, two columns
  const std::string data("\x01\x02\x03\x01\x02\x03", 6);
  EXPECT_EQ(apply_predictor(data, 2, 3, 8, 2),
            std::string("\x01\x02\x03\x02\x04\x06", 6));
}

TEST(PdfFilter, decode_no_filter) {
  const DecodeResult result = decode(Object(), Object(), "hello");
  EXPECT_EQ(result.data, "hello");
  EXPECT_FALSE(result.stopped_at_filter.has_value());
}

TEST(PdfFilter, decode_flate) {
  const DecodeResult result =
      decode(name("FlateDecode"), Object(), zlib_deflate("hello"));
  EXPECT_EQ(result.data, "hello");
}

TEST(PdfFilter, decode_flate_abbreviation) {
  const DecodeResult result =
      decode(name("Fl"), Object(), zlib_deflate("hello"));
  EXPECT_EQ(result.data, "hello");
}

TEST(PdfFilter, decode_flate_with_png_predictor) {
  // the layout every xref stream uses: Flate around PNG Up rows
  const std::string predicted("\x02\x01\x01\x01\x01"
                              "\x02\x01\x01\x01\x01",
                              10);
  const DecodeResult result =
      decode(name("FlateDecode"),
             dictionary({{"Predictor", Object(Integer(12))},
                         {"Columns", Object(Integer(4))}}),
             zlib_deflate(predicted));
  EXPECT_EQ(result.data, std::string("\x01\x01\x01\x01\x02\x02\x02\x02", 8));
}

TEST(PdfFilter, decode_chain) {
  // [ /AHx /RL ]: hex encoding of the run-length example above
  const DecodeResult result =
      decode(array({name("AHx"), name("RL")}), Object(), "02616263FE7A80>");
  EXPECT_EQ(result.data, "abczzz");
}

TEST(PdfFilter, decode_stops_at_image_codec) {
  const Object parms = dictionary({{"ColorTransform", Object(Integer(0))}});
  const DecodeResult result =
      decode(array({name("FlateDecode"), name("DCTDecode")}),
             array({Object(), parms}), zlib_deflate("jpeg payload"));
  ASSERT_TRUE(result.stopped_at_filter.has_value());
  EXPECT_EQ(*result.stopped_at_filter, "DCTDecode");
  EXPECT_EQ(result.data, "jpeg payload");
  ASSERT_TRUE(result.stopped_at_parms.is_dictionary());
  EXPECT_EQ(
      result.stopped_at_parms.as_dictionary()["ColorTransform"].as_integer(),
      0);
}

TEST(PdfFilter, decode_crypt_identity_passes_through) {
  const DecodeResult result =
      decode(name("Crypt"), dictionary({{"Name", name("Identity")}}), "data");
  EXPECT_EQ(result.data, "data");
}

TEST(PdfFilter, decode_crypt_other_throws) {
  EXPECT_THROW(
      decode(name("Crypt"), dictionary({{"Name", name("StdCF")}}), "data"),
      std::runtime_error);
}

TEST(PdfFilter, decode_unknown_filter_throws) {
  EXPECT_THROW(decode(name("NoSuchDecode"), Object(), "data"),
               std::runtime_error);
}
