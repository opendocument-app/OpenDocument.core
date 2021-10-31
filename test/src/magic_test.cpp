#include <gtest/gtest.h>
#include <magic.h>
#include <memory>
#include <odr/file.h>
#include <test_util.h>

using namespace odr;
using namespace odr::test;

TEST(magic, odt) {
  File file(TestData::test_file_path("odr-public/odt/about.odt"));
  EXPECT_EQ(magic::file_type(*file.impl()), FileType::zip);
}

TEST(magic, doc) {
  File file(TestData::test_file_path("odr-public/doc/empty.doc"));
  EXPECT_EQ(magic::file_type(*file.impl()),
            FileType::compound_file_binary_format);
}

TEST(magic, svm) {
  File file(TestData::test_file_path("odr-public/svm/chart-1.svm"));
  EXPECT_EQ(magic::file_type(*file.impl()), FileType::starview_metafile);
}

TEST(magic, odf) {
  File file(TestData::test_file_path("odr-private/pdf/sample.pdf"));
  EXPECT_EQ(magic::file_type(*file.impl()), FileType::portable_document_format);
}
