#include <access/Path.h>
#include <gtest/gtest.h>
#include <unordered_map>

namespace odr {

TEST(Path, empty) { EXPECT_EQ("", Path().string()); }

TEST(Path, join) {
  Path a("ppt/slides");
  Path b("../media/image8.png");

  EXPECT_EQ("ppt/media/image8.png", a.join(b).string());
}

} // namespace odr
