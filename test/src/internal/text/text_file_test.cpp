#include <gtest/gtest.h>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/internal/text/text_file.hpp>
#include <test_util.hpp>

using namespace odr;
using namespace odr::test;

TEST(TextFile, odt) {
  File file(TestData::test_file_path("odr-public/odt/about.odt"));
  EXPECT_THROW(internal::text::TextFile(file.impl()), UnknownCharset);
}

TEST(TextFile, txt) {
  File file(TestData::test_file_path("odr-public/txt/lorem ipsum.txt"));
}
