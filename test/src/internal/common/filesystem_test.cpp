#include <odr/internal/common/filesystem.hpp>

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/common/path.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

using namespace odr::internal;

namespace {

/// The paths a walk over `paths` visits, in order.
std::vector<std::string> walk(const abstract::ReadableFilesystem &filesystem,
                              const AbsPath &root) {
  std::vector<std::string> result;
  for (const auto walker = filesystem.file_walker(root); !walker->end();
       walker->next()) {
    result.push_back(walker->path().string());
  }
  return result;
}

VirtualFilesystem
filesystem_of(const std::vector<std::string> &files,
              const std::vector<std::string> &directories = {}) {
  VirtualFilesystem result;
  for (const std::string &directory : directories) {
    result.create_directory(AbsPath(directory));
  }
  for (const std::string &file : files) {
    result.copy(std::make_shared<MemoryFile>(std::string()), AbsPath(file));
  }
  return result;
}

} // namespace

TEST(VirtualFilesystem, walker_reports_the_depth_of_every_entry) {
  const VirtualFilesystem filesystem =
      filesystem_of({"/mimetype", "/a/b.txt", "/a/c/d.txt"});

  const auto walker = filesystem.file_walker(AbsPath("/"));

  EXPECT_EQ(walker->path().string(), "/a/b.txt");
  EXPECT_EQ(walker->depth(), 1);
  walker->next();
  EXPECT_EQ(walker->path().string(), "/a/c/d.txt");
  EXPECT_EQ(walker->depth(), 2);
  walker->next();
  EXPECT_EQ(walker->path().string(), "/mimetype");
  EXPECT_EQ(walker->depth(), 0);
}

TEST(VirtualFilesystem, walker_depth_is_relative_to_the_walked_root) {
  const VirtualFilesystem filesystem = filesystem_of({"/a/c/d.txt"});

  const auto walker = filesystem.file_walker(AbsPath("/a"));

  EXPECT_EQ(walker->depth(), 1);
}

TEST(VirtualFilesystem, flat_next_skips_the_subtree_and_terminates) {
  const VirtualFilesystem filesystem =
      filesystem_of({"/a/b.txt", "/a/c/d.txt", "/e.txt"});

  const auto walker = filesystem.file_walker(AbsPath("/"));

  EXPECT_EQ(walker->path().string(), "/a/b.txt");
  walker->flat_next();
  EXPECT_EQ(walker->path().string(), "/a/c/d.txt");
  walker->flat_next();
  EXPECT_EQ(walker->path().string(), "/e.txt");
  walker->flat_next();
  EXPECT_TRUE(walker->end());
}

/// The loop flat_next() exists for; it never terminated while flat_next() did
/// not advance.
TEST(VirtualFilesystem, flat_next_over_a_directory_skips_what_is_under_it) {
  const VirtualFilesystem filesystem =
      filesystem_of({"/a/b.txt", "/a/c.txt", "/e.txt"}, {"/a"});

  const auto walker = filesystem.file_walker(AbsPath("/"));

  EXPECT_EQ(walker->path().string(), "/a");
  EXPECT_TRUE(walker->is_directory());
  walker->flat_next();
  EXPECT_EQ(walker->path().string(), "/e.txt");
}

/// "/a-b" sorts between "/a" and "/a/b" by string, so a subtree is only
/// contiguous in a component-wise order.
TEST(VirtualFilesystem, flat_next_skips_a_subtree_a_sibling_sorts_into) {
  const VirtualFilesystem filesystem =
      filesystem_of({"/a/b.txt", "/a-b.txt", "/e.txt"}, {"/a"});

  EXPECT_EQ(walk(filesystem, AbsPath("/")),
            (std::vector<std::string>{"/a", "/a/b.txt", "/a-b.txt", "/e.txt"}));

  const auto walker = filesystem.file_walker(AbsPath("/"));
  walker->flat_next();
  EXPECT_EQ(walker->path().string(), "/a-b.txt");
}

TEST(VirtualFilesystem, pop_leaves_the_directory) {
  const VirtualFilesystem filesystem =
      filesystem_of({"/a/b.txt", "/a/c.txt", "/e.txt"});

  const auto walker = filesystem.file_walker(AbsPath("/"));

  EXPECT_EQ(walker->path().string(), "/a/b.txt");
  walker->pop();
  EXPECT_EQ(walker->path().string(), "/e.txt");
}

TEST(VirtualFilesystem, pop_at_depth_zero_ends_the_walk) {
  const VirtualFilesystem filesystem = filesystem_of({"/a.txt", "/b.txt"});

  const auto walker = filesystem.file_walker(AbsPath("/"));

  EXPECT_EQ(walker->depth(), 0);
  walker->pop();
  EXPECT_TRUE(walker->end());
}

/// An archive names its entries in its own order, and `as_filesystem()`
/// inserts them in it. A directory that arrives after what it holds is still an
/// entry of its own, or the walk would not offer it to `flat_next()`.
TEST(VirtualFilesystem,
     a_directory_entry_survives_arriving_after_its_children) {
  VirtualFilesystem filesystem;
  filesystem.copy(std::make_shared<MemoryFile>(std::string()),
                  AbsPath("/a/b.txt"));
  EXPECT_TRUE(filesystem.create_directory(AbsPath("/a")));

  EXPECT_EQ(walk(filesystem, AbsPath("/")),
            (std::vector<std::string>{"/a", "/a/b.txt"}));

  const auto walker = filesystem.file_walker(AbsPath("/"));
  EXPECT_TRUE(walker->is_directory());
  walker->flat_next();
  EXPECT_TRUE(walker->end());
}

TEST(VirtualFilesystem, an_intermediate_directory_is_a_directory) {
  const VirtualFilesystem filesystem = filesystem_of({"/a/c/d.txt"});

  EXPECT_TRUE(filesystem.exists(AbsPath("/")));
  EXPECT_TRUE(filesystem.is_directory(AbsPath("/")));
  EXPECT_TRUE(filesystem.exists(AbsPath("/a")));
  EXPECT_TRUE(filesystem.is_directory(AbsPath("/a")));
  EXPECT_TRUE(filesystem.exists(AbsPath("/a/c")));
  EXPECT_TRUE(filesystem.is_directory(AbsPath("/a/c")));

  EXPECT_FALSE(filesystem.is_file(AbsPath("/a")));
  EXPECT_FALSE(filesystem.exists(AbsPath("/b")));
  EXPECT_FALSE(filesystem.is_directory(AbsPath("/a/c/d.txt")));
}
