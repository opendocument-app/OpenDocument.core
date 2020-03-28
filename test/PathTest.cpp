#include <access/Path.h>
#include <gtest/gtest.h>
#include <unordered_map>

TEST(Path, empty) { EXPECT_EQ("", odr::access::Path().string()); }

TEST(Path, join) {
  const odr::access::Path a("ppt/slides");
  const odr::access::Path b("../media/image8.png");

  EXPECT_EQ("ppt/media/image8.png", a.join(b).string());
}
