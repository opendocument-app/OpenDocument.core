#include <access/Path.h>
#include <gtest/gtest.h>

using namespace odr::access;

TEST(Path, empty) { EXPECT_EQ("", Path().string()); }

TEST(Path, root) { EXPECT_EQ("/", Path("/").string()); }

TEST(Path, join) {
  const Path a("ppt/slides");
  const Path b("../media/image8.png");

  EXPECT_EQ("ppt/media/image8.png", a.join(b).string());
}

TEST(Path, normalization) {
  EXPECT_EQ(Path("ppt/media/image8.png"), Path("./ppt/media/image8.png"));
  EXPECT_EQ(Path("ppt/media/image8.png"), Path("ppt/./media/image8.png"));
  EXPECT_EQ(Path("ppt/media/image8.png"), Path("ppt/media/./image8.png"));
  EXPECT_EQ(Path("ppt/media/image8.png"),
            Path("././././././ppt/media/image8.png"));
}
