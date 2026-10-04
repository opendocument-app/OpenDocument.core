#include <odr/internal/util/byte_stream_util.hpp>
#include <odr/internal/util/byte_string.hpp>
#include <odr/internal/util/stream_util.hpp>

#include <ios>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

using namespace odr::internal::util;

TEST(ByteString, patch_offsets_are_checked_before_addition) {
  std::string bytes(4, '\0');
  byte_string::write_u32_be(bytes, 0, 0x12345678);
  EXPECT_EQ(bytes, "\x12\x34\x56\x78");
  byte_string::write_u16_be(bytes, 2, 0xabcd);
  EXPECT_EQ(bytes, "\x12\x34\xab\xcd");
  for (const std::size_t offset :
       {std::size_t{4}, std::numeric_limits<std::size_t>::max()}) {
    EXPECT_THROW(byte_string::write_u16_be(bytes, offset, 0),
                 std::runtime_error);
    EXPECT_THROW(byte_string::write_u32_be(bytes, offset, 0),
                 std::runtime_error);
  }
}

// A `ViewStream` is seekable: pdf object streams address their members by
// absolute position rather than reading them in order.
TEST(ViewStream, seek) {
  const std::string_view view("0123456789");
  stream::ViewStream in(view);

  in.seekg(4);
  EXPECT_EQ(in.tellg(), 4);
  EXPECT_EQ(stream::read(in, 3), "456");

  in.seekg(-2, std::ios::cur);
  EXPECT_EQ(stream::read(in, 2), "56");

  in.seekg(-1, std::ios::end);
  EXPECT_EQ(stream::read(in, 1), "9");

  in.seekg(0);
  EXPECT_EQ(stream::read(in), "0123456789");
}

// An out-of-range seek fails the stream instead of moving the cursor.
TEST(ViewStream, seek_out_of_range) {
  const std::string_view view("0123456789");
  stream::ViewStream in(view);

  in.seekg(11);
  EXPECT_TRUE(in.fail());

  in.clear();
  in.seekg(-1, std::ios::beg);
  EXPECT_TRUE(in.fail());

  in.clear();
  in.seekg(std::numeric_limits<std::streamoff>::max(), std::ios::end);
  EXPECT_TRUE(in.fail());

  in.clear();
  in.seekg(std::numeric_limits<std::streamoff>::min(), std::ios::cur);
  EXPECT_TRUE(in.fail());
}

TEST(ViewStream, empty_view_is_seekable) {
  stream::ViewStream in(std::string_view{});
  EXPECT_EQ(in.tellg(), 0);
  in.seekg(0, std::ios::end);
  EXPECT_TRUE(in.good());
  EXPECT_EQ(stream::read(in), "");
}

TEST(Stream, delimiter_reads_preserve_failed_input) {
  std::istringstream in("first\nsecond;");
  in.setstate(std::ios::failbit);
  EXPECT_EQ(stream::read_line(in, false), "");
  EXPECT_EQ(stream::read_until(in, ';', false), "");
  in.clear();
  EXPECT_EQ(stream::read_line(in, false), "first");
  EXPECT_EQ(stream::read_until(in, ';', true), "second;");
}

TEST(Stream, copying_reports_input_and_output_failures) {
  std::istringstream in(std::string(4097, 'x'));
  std::ostringstream out;
  stream::pipe(in, out);
  EXPECT_EQ(out.str(), std::string(4097, 'x'));

  in.clear();
  in.seekg(0);
  out.setstate(std::ios::badbit);
  EXPECT_THROW(stream::pipe(in, out), std::ios_base::failure);

  in.setstate(std::ios::badbit);
  EXPECT_THROW(stream::read(in), std::ios_base::failure);
  EXPECT_THROW(stream::read(in, 1), std::ios_base::failure);

  class FailingBuffer final : public std::streambuf {
    int_type underflow() override { throw std::runtime_error("read failure"); }
  } buffer;
  std::istream broken(&buffer);
  EXPECT_THROW(stream::read(broken), std::ios_base::failure);
}

TEST(ByteStream, length_prefixed_reads_handle_chunks_and_stream_exceptions) {
  const std::string data(4097, 'x');
  std::istringstream in(data);
  in.exceptions(std::ios::badbit | std::ios::failbit);
  EXPECT_EQ(byte_stream::read_u8s(in, data.size()), data);
  EXPECT_THROW(byte_stream::read_u8s(in, 1), std::ios_base::failure);
}

// Nothing reaches the stream until the buffer is released, and the release
// writes the prologue the held bytes need in front of them.
TEST(DeferredBuffer, holds_until_released) {
  std::ostringstream out;
  stream::DeferredBuffer buffer(out, 1024, [&out] { out << "head"; });
  std::ostream deferred(&buffer);

  deferred << "body";
  EXPECT_EQ(out.str(), "");

  buffer.release();
  EXPECT_EQ(out.str(), "headbody");

  deferred << "tail";
  EXPECT_EQ(out.str(), "headbodytail");
}

// Past the cap it releases itself, so what it holds is bounded.
TEST(DeferredBuffer, releases_itself_past_the_cap) {
  std::ostringstream out;
  stream::DeferredBuffer buffer(out, 4, [&out] { out << "head"; });
  std::ostream deferred(&buffer);

  deferred << "abc";
  EXPECT_EQ(out.str(), "");

  deferred << "de";
  EXPECT_EQ(out.str(), "headabcde");
}

// Releasing twice writes the prologue once.
TEST(DeferredBuffer, releases_once) {
  std::ostringstream out;
  stream::DeferredBuffer buffer(out, 1024, [&out] { out << "head"; });
  std::ostream deferred(&buffer);

  deferred << "body";
  buffer.release();
  buffer.release();

  EXPECT_EQ(out.str(), "headbody");
}

TEST(DeferredBuffer, propagates_sink_failures_before_and_after_release) {
  for (const bool released : {false, true}) {
    std::ostringstream out;
    stream::DeferredBuffer buffer(out, 0, [] {});
    std::ostream deferred(&buffer);
    if (released) {
      buffer.release();
    }
    out.setstate(std::ios::badbit);
    deferred << "body";
    EXPECT_TRUE(deferred.bad());
    deferred.clear();
    deferred.put('x');
    EXPECT_TRUE(deferred.bad());
  }
}
