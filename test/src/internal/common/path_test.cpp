#include <gtest/gtest.h>
#include <internal/common/path.h>
#include <memory>

using namespace odr::internal::common;

TEST(Path, empty) { EXPECT_EQ("", Path().string()); }

TEST(Path, root) { EXPECT_EQ("/", Path("/").string()); }

TEST(Path, normalization) {
  EXPECT_EQ(Path("ppt/media/image8.png"), Path("./ppt/media/image8.png"));
  EXPECT_EQ(Path("ppt/media/image8.png"), Path("ppt/./media/image8.png"));
  EXPECT_EQ(Path("ppt/media/image8.png"), Path("ppt/media/./image8.png"));
  EXPECT_EQ(Path("ppt/media/image8.png"),
            Path("././././././ppt/media/image8.png"));
}

TEST(Path, join) {
  const Path a("ppt/slides");
  const Path b("../media/image8.png");

  EXPECT_EQ("ppt/media/image8.png", a.join(b).string());
}

TEST(Path, join_root) {
  const Path a("/");
  const Path b("tmp");

  EXPECT_EQ("/tmp", a.join(b).string());
}

TEST(Path, root_parent) { EXPECT_ANY_THROW(Path("/").parent()); }

TEST(Path, one_parent) { EXPECT_EQ("/", Path("/tmp").parent().string()); }

TEST(Path, rebase) {
  EXPECT_EQ("image8.png",
            Path("./ppt/media/image8.png").rebase("ppt/media").string());
}
