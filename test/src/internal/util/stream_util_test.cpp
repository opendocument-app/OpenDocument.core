#include <odr/internal/util/stream_util.hpp>

#include <ios>
#include <sstream>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

using namespace odr::internal::util;

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
