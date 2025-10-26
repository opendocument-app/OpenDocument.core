#include <odr/exceptions.hpp>
#include <odr/odr.hpp>

#include <odr/internal/project_info.hpp>

#include <test_util.hpp>

#include <gtest/gtest.h>

using namespace odr;
using namespace odr::internal;
using namespace odr::test;

TEST(odr, version) { EXPECT_TRUE(odr::version().empty()); }

TEST(odr, commit) { EXPECT_FALSE(odr::commit_hash().empty()); }

TEST(odr, types_odt) {
  const auto logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  const auto path = TestData::test_file_path("odr-public/odt/about.odt");
  const auto types = list_file_types(path, *logger);
  EXPECT_EQ(types.size(), 2);
  EXPECT_EQ(types[0], FileType::zip);
  EXPECT_EQ(types[1], FileType::opendocument_text);

  const auto mime = mimetype(path, *logger);
  EXPECT_EQ(mime, "application/vnd.oasis.opendocument.text");
}

TEST(odr, types_wpd) {
  const auto logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  const auto path =
      TestData::test_file_path("odr-public/wpd/Sync3 Sample Page.wpd");
  const auto types = list_file_types(path, *logger);
  EXPECT_EQ(types.size(), 1);
  EXPECT_EQ(types[0], FileType::word_perfect);

  if (project_info::has_libmagic()) {
    const auto mime = mimetype(path, *logger);
    EXPECT_EQ(mime, "application/vnd.wordperfect");
  } else {
    EXPECT_THROW(std::ignore = mimetype(path, *logger), UnsupportedFileType);
  }
}
