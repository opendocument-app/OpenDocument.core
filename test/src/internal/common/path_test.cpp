#include <gtest/gtest.h>
#include <memory>
#include <odr/internal/common/path.h>

using namespace odr::internal::common;

TEST(Path, empty) { EXPECT_EQ("", Path().string()); }

TEST(Path, root) { EXPECT_EQ("/", Path("/").string()); }

TEST(Path, something) {
  EXPECT_EQ("/some/thing", Path("/some/thing").string());
}

TEST(Path, normalization) {
  EXPECT_EQ(Path("ppt/media/image8.png"), Path("./ppt/media/image8.png"));
  EXPECT_EQ(Path("ppt/media/image8.png"), Path("ppt/./media/image8.png"));
  EXPECT_EQ(Path("ppt/media/image8.png"), Path("ppt/media/./image8.png"));
  EXPECT_EQ(Path("ppt/media/image8.png"),
            Path("././././././ppt/media/image8.png"));
}

TEST(Path, join) {
  EXPECT_EQ("/home/media/image8.png",
            Path("/home").join("media/image8.png").string());
}

TEST(Path, join_relative) {
  EXPECT_EQ("ppt/media/image8.png",
            Path("ppt/slides").join("../media/image8.png").string());
}

TEST(Path, join_root) { EXPECT_EQ("/tmp", Path("/").join("tmp").string()); }

TEST(Path, root_parent) { EXPECT_ANY_THROW(auto tmp = Path("/").parent()); }

TEST(Path, one_parent) { EXPECT_EQ("/", Path("/tmp").parent().string()); }

TEST(Path, rebase) {
  EXPECT_EQ("image8.png",
            Path("./ppt/media/image8.png").rebase("ppt/media").string());
}

TEST(Path, rebase_separate_tree) {
  EXPECT_EQ("../../other/directory", Path("ppt/media/other/directory")
                                         .rebase("./ppt/media/some/directory")
                                         .string());
}

TEST(Path, rebase_relative) {
  EXPECT_EQ("other/directory",
            Path("../../other/directory").rebase("../..").string());
}

TEST(Path, common_root) {
  EXPECT_EQ("ppt/media",
            Path("./ppt/media/image8.png").common_root("ppt/media").string());
}

TEST(Path, common_root_relative) {
  EXPECT_EQ("../..",
            Path("../../other/directory").common_root("../..").string());
}
