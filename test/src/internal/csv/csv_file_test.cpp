#include <odr/exceptions.hpp>
#include <odr/file.hpp>

#include <test_util.hpp>

#include <gtest/gtest.h>

#include <odr/internal/csv/csv_file.hpp>

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

TEST(CsvFile, csv) {
  File file(
      TestData::test_file_path("odr-public/csv/file_example_ODS_5000.csv"));
}
