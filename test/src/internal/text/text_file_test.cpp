#include <gtest/gtest.h>
#include <internal/text/text_file.h>
#include <odr/exceptions.h>
#include <odr/file.h>
#include <test_util.h>

using namespace odr;
using namespace odr::test;

TEST(TextFile, odt) {
  File file(TestData::test_file_path("odr-public/odt/about.odt"));
  EXPECT_THROW(internal::text::TextFile(file.impl()), UnknownCharset);
}
