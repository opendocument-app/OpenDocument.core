#include <test_util.hpp>

#include <gtest/gtest.h>

#include <csv.hpp>

using namespace odr;
using namespace odr::test;

TEST(Csv, guess_format) {
  const auto path =
      TestData::test_file_path("odr-public/csv/file_example_ODS_5000.csv");
  const auto [delim, header_row] = csv::guess_format(path);

  EXPECT_EQ(0, header_row);
  EXPECT_EQ(',', delim);
}

TEST(Csv, CSVReader_csv) {
  const auto path =
      TestData::test_file_path("odr-public/csv/file_example_ODS_5000.csv");

  const csv::CSVReader reader(path);
  const auto format = reader.get_format();

  EXPECT_EQ(0, format.get_header());
  EXPECT_EQ(',', format.get_delim());
  EXPECT_EQ(0, reader.n_rows());
  EXPECT_EQ(8, reader.get_col_names().size());
}

TEST(Csv, CSVReader_txt) {
  // TODO: txt is handled as csv

  const auto path = TestData::test_file_path("odr-public/txt/lorem ipsum.txt");

  const csv::CSVReader reader(path);
  const auto format = reader.get_format();

  EXPECT_EQ(1, format.get_header());
  EXPECT_EQ(',', format.get_delim());
  EXPECT_EQ(0, reader.n_rows());
  EXPECT_EQ(12, reader.get_col_names().size());
}
