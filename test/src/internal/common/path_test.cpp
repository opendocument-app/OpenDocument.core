#include <odr/internal/common/path.hpp>

#include <gtest/gtest.h>

#include <memory>

using namespace odr::internal;

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

TEST(Path, basename) {
  EXPECT_EQ("image8.png", Path("/ppt/media/image8.png").basename());
  EXPECT_EQ("mimetype", Path("mimetype").basename());
}

TEST(Path, extension) {
  // The last dot-separated segment, without the leading dot.
  EXPECT_EQ("png", Path("/ppt/media/image8.png").extension());
  EXPECT_EQ("ppt", Path("foo.ppt").extension());
  // Multiple dots in the name: only the final extension.
  EXPECT_EQ("ppt", Path("a.b/2025-09-11.15_35_15.ppt").extension());
  EXPECT_EQ("gz", Path("archive.tar.gz").extension());
  // No extension, and dot-files (a leading dot is not an extension).
  EXPECT_EQ("", Path("mimetype").extension());
  EXPECT_EQ("", Path(".bashrc").extension());
}

TEST(Path, join) {
  EXPECT_EQ("/home/media/image8.png",
            Path("/home").join(RelPath("media/image8.png")).string());
}

TEST(Path, join_relative) {
  EXPECT_EQ("ppt/media/image8.png",
            Path("ppt/slides").join(RelPath("../media/image8.png")).string());
}

TEST(Path, join_root) {
  EXPECT_EQ("/tmp", Path("/").join(RelPath("tmp")).string());
}

TEST(Path, root_parent) { EXPECT_ANY_THROW(auto tmp = Path("/").parent()); }

TEST(Path, one_parent) { EXPECT_EQ("/", Path("/tmp").parent().string()); }

TEST(Path, rebase) {
  EXPECT_EQ("image8.png",
            Path("./ppt/media/image8.png").rebase(Path("ppt/media")).string());
}

TEST(Path, rebase_to_relative) {
  EXPECT_EQ("mimetype", Path("/mimetype").rebase(Path("/")).string());
}

TEST(Path, rebase_separate_tree) {
  EXPECT_EQ("../../other/directory",
            Path("ppt/media/other/directory")
                .rebase(Path("./ppt/media/some/directory"))
                .string());
}

TEST(Path, rebase_relative) {
  EXPECT_EQ("other/directory",
            Path("../../other/directory").rebase(Path("../..")).string());
}

TEST(Path, common_root_simple) {
  EXPECT_EQ("/", Path("/").common_root(Path("/mimetype")).string());
}

TEST(Path, common_root) {
  EXPECT_EQ(
      "ppt/media",
      Path("./ppt/media/image8.png").common_root(Path("ppt/media")).string());
}

TEST(Path, common_root_relative) {
  EXPECT_EQ("../..",
            Path("../../other/directory").common_root(Path("../..")).string());
}
