#include <odr/internal/util/stream_util.hpp>

#include <ios>
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
