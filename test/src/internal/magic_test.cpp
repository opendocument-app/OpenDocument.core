#include <odr/file.hpp>

#include <odr/internal/magic.hpp>
#include <odr/internal/project_info.hpp>

#include <test_util.hpp>

#include <gtest/gtest.h>

using namespace odr;
using namespace odr::internal;
using namespace odr::test;

TEST(magic, odt) {
  const File file(TestData::test_file_path("odr-public/odt/about.odt"));
  EXPECT_EQ(magic::file_type(*file.impl()), FileType::zip);

  if (project_info::has_libmagic()) {
    EXPECT_EQ(magic::mimetype(file.disk_path().value()),
              "application/vnd.oasis.opendocument.text");
  }
}

TEST(magic, doc) {
  const File file(TestData::test_file_path("odr-public/doc/empty.doc"));
  EXPECT_EQ(magic::file_type(*file.impl()),
            FileType::compound_file_binary_format);

  if (project_info::has_libmagic()) {
    EXPECT_EQ(magic::mimetype(file.disk_path().value()), "application/msword");
  }
}

TEST(magic, svm) {
  const File file(TestData::test_file_path("odr-public/svm/chart-1.svm"));
  EXPECT_EQ(magic::file_type(*file.impl()), FileType::starview_metafile);

  if (project_info::has_libmagic()) {
    EXPECT_EQ(magic::mimetype(file.disk_path().value()),
              "application/octet-stream");
  }
}

TEST(magic, odf) {
  const File file(TestData::test_file_path("odr-private/pdf/sample.pdf"));
  EXPECT_EQ(magic::file_type(*file.impl()), FileType::portable_document_format);

  if (project_info::has_libmagic()) {
    EXPECT_EQ(magic::mimetype(file.disk_path().value()), "application/pdf");
  }
}

TEST(magic, wpd) {
  const File file(
      TestData::test_file_path("odr-public/wpd/Sync3 Sample Page.wpd"));
  EXPECT_EQ(magic::file_type(*file.impl()), FileType::word_perfect);

  if (project_info::has_libmagic()) {
    EXPECT_EQ(magic::mimetype(file.disk_path().value()),
              "application/vnd.wordperfect");
  }
}
