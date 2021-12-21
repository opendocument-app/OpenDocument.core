#include <gtest/gtest.h>
#include <internal/csv/csv_file.h>
#include <internal/text/text_file.h>
#include <odr/exceptions.h>
#include <odr/file.h>
#include <test_util.h>

using namespace odr;
using namespace odr::test;

TEST(CsvFile, odt) {
  File file(TestData::test_file_path("odr-public/odt/about.odt"));
  EXPECT_THROW(internal::csv::CsvFile(
                   std::make_shared<internal::text::TextFile>(file.impl())),
               std::runtime_error);
}

TEST(CsvFile, txt) {
  File file(TestData::test_file_path("odr-public/txt/lorem ipsum.txt"));
  EXPECT_THROW(internal::csv::CsvFile(
                   std::make_shared<internal::text::TextFile>(file.impl())),
               std::runtime_error);
}
