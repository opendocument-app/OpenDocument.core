#include <odr/internal/iwork/iwork_snappy.hpp>

#include <string>

#include <gtest/gtest.h>

using namespace odr::internal::iwork;

namespace {

/// A Snappy block: the uncompressed length as a varint, then @p body.
std::string block(const std::size_t uncompressed_length,
                  const std::string &body) {
  std::string result;
  for (std::size_t rest = uncompressed_length;;) {
    const auto byte = static_cast<char>(rest & 0x7f);
    rest >>= 7;
    result.push_back(rest == 0 ? byte : static_cast<char>(byte | 0x80));
    if (rest == 0) {
      break;
    }
  }
  return result + body;
}

/// A literal tag for @p text, in the form that carries the length inline.
std::string literal(const std::string &text) {
  return std::string(1, static_cast<char>((text.size() - 1) << 2)) + text;
}

/// One `.iwa` block header plus @p body.
std::string framed(const std::string &body) {
  const std::size_t length = body.size();
  const std::string header{'\0', static_cast<char>(length & 0xff),
                           static_cast<char>((length >> 8) & 0xff),
                           static_cast<char>((length >> 16) & 0xff)};
  return header + body;
}

} // namespace

TEST(SnappyDecompressBlock, literal) {
  EXPECT_EQ(snappy_decompress_block(block(5, literal("hello"))), "hello");
}

TEST(SnappyDecompressBlock, empty) {
  EXPECT_EQ(snappy_decompress_block(block(0, "")), "");
}

// A literal of 61 bytes or more names its length in the bytes after the tag.
TEST(SnappyDecompressBlock, long_literal) {
  const std::string text(300, 'x');
  const std::string body = std::string{'\xf4', '\x2b', '\x01'} + text;
  EXPECT_EQ(snappy_decompress_block(block(text.size(), body)), text);
}

// Copy tag 1: a three-bit length and a ten-bit offset.
TEST(SnappyDecompressBlock, copy_with_one_byte_offset) {
  const std::string body = literal("abc") + std::string{'\x09', '\x03'};
  EXPECT_EQ(snappy_decompress_block(block(9, body)), "abcabcabc");
}

// Copy tag 2: a six-bit length and a two-byte offset.
TEST(SnappyDecompressBlock, copy_with_two_byte_offset) {
  const std::string body =
      literal("abcd") + std::string{'\x0e', '\x04', '\x00'};
  EXPECT_EQ(snappy_decompress_block(block(8, body)), "abcdabcd");
}

// The run a copy reads may be the one it is writing.
TEST(SnappyDecompressBlock, overlapping_copy) {
  const std::string body = literal("ab") + std::string{'\x09', '\x02'};
  EXPECT_EQ(snappy_decompress_block(block(8, body)), "abababab");
}

TEST(SnappyDecompressBlock, length_does_not_match) {
  EXPECT_ANY_THROW(std::ignore =
                       snappy_decompress_block(block(6, literal("hello"))));
}

TEST(SnappyDecompressBlock, literal_runs_past_the_block) {
  EXPECT_ANY_THROW(std::ignore = snappy_decompress_block(
                       block(5, std::string{'\x10'} + "hel")));
}

TEST(SnappyDecompressBlock, copy_points_outside_the_block) {
  const std::string body = literal("abc") + std::string{'\x09', '\x09'};
  EXPECT_ANY_THROW(std::ignore = snappy_decompress_block(block(9, body)));
}

TEST(SnappyDecompressBlock, length_varint_does_not_terminate) {
  EXPECT_ANY_THROW(std::ignore = snappy_decompress_block("\x80\x80\x80"));
}

TEST(IwaDecompress, one_block) {
  EXPECT_EQ(iwa_decompress(framed(block(5, literal("hello")))), "hello");
}

// A file is as many blocks as it takes; they concatenate.
TEST(IwaDecompress, two_blocks) {
  const std::string data =
      framed(block(5, literal("hello"))) + framed(block(6, literal(" world")));
  EXPECT_EQ(iwa_decompress(data), "hello world");
}

TEST(IwaDecompress, empty_file) { EXPECT_EQ(iwa_decompress(""), ""); }

TEST(IwaDecompress, header_is_not_zero) {
  std::string data = framed(block(5, literal("hello")));
  data[0] = '\x01';
  EXPECT_ANY_THROW(std::ignore = iwa_decompress(data));
}

TEST(IwaDecompress, truncated_mid_block) {
  const std::string data = framed(block(5, literal("hello")));
  EXPECT_ANY_THROW(std::ignore =
                       iwa_decompress(data.substr(0, data.size() - 2)));
}

TEST(IwaDecompress, truncated_header) {
  EXPECT_ANY_THROW(std::ignore = iwa_decompress(std::string{'\0', '\x07'}));
}
