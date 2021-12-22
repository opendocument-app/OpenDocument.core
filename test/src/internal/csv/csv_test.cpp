#include <csv.hpp>
#include <gtest/gtest.h>
#include <test_util.h>

using namespace odr;
using namespace odr::test;

TEST(Csv, guess_format) {
  const auto path =
      TestData::test_file_path("odr-public/csv/file_example_ODS_5000.csv");
  const auto format = csv::guess_format(path);

  EXPECT_EQ(0, format.header_row);
  EXPECT_EQ(',', format.delim);
}

TEST(Csv, CSVReader_csv) {
  const auto path =
      TestData::test_file_path("odr-public/csv/file_example_ODS_5000.csv");

  csv::CSVReader reader(path);
  const auto format = reader.get_format();

  EXPECT_EQ(0, format.get_header());
  EXPECT_EQ(',', format.get_delim());
  EXPECT_EQ(0, reader.num_rows);
  EXPECT_EQ(8, reader.get_col_names().size());
}

TEST(Csv, CSVReader_txt) {
  // TODO: txt is handled as csv

  const auto path = TestData::test_file_path("odr-public/txt/lorem ipsum.txt");

  csv::CSVReader reader(path);
  const auto format = reader.get_format();

  EXPECT_EQ(0, format.get_header());
  EXPECT_EQ(',', format.get_delim());
  EXPECT_EQ(0, reader.num_rows);
  EXPECT_EQ(6, reader.get_col_names().size());
}
