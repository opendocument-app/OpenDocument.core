#include <odr/exceptions.hpp>
#include <odr/file.hpp>

#include <test_util.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <odr/internal/csv/csv_file.hpp>

#include <memory>
#include <stdexcept>
#include <string>

using namespace odr;
using namespace odr::test;

TEST(CsvFile, odt) {
  const File file(TestData::test_file_path("odr-public/odt/about.odt"));
  EXPECT_THROW(internal::csv::CsvFile(
                   std::make_shared<internal::text::TextFile>(file.impl())),
               std::runtime_error);
}

TEST(CsvFile, txt) {
  const File file(TestData::test_file_path("odr-public/txt/lorem ipsum.txt"));
  EXPECT_THROW(internal::csv::CsvFile(
                   std::make_shared<internal::text::TextFile>(file.impl())),
               std::runtime_error);
}

TEST(CsvFile, csv) {
  const File file(
      TestData::test_file_path("odr-public/csv/file_example_ODS_5000.csv"));

  EXPECT_NO_THROW(internal::csv::CsvFile(
      std::make_shared<internal::text::TextFile>(file.impl())));

  // and the probe in `open_strategy` reaches the same conclusion
  EXPECT_THAT(DecodedFile::list_file_types(file),
              testing::Contains(FileType::comma_separated_values));
}

TEST(CsvFile, records_must_have_a_consistent_field_count) {
  const auto check = [](const std::string &content) {
    internal::csv::CsvFile(std::make_shared<internal::text::TextFile>(
        File::from_memory(content).impl()));
  };

  EXPECT_NO_THROW(check("a,b,c\n1,2,3\n"));
  EXPECT_NO_THROW(check("a,b,c\n1,2,3"));        // no trailing newline
  EXPECT_NO_THROW(check("a,b\r\n1,2\r\n"));      // crlf
  EXPECT_NO_THROW(check("a,b\n\"x,y\",2\n"));    // a quoted delimiter
  EXPECT_NO_THROW(check("a,b\n\"x\ny\",2\n"));   // a quoted newline
  EXPECT_NO_THROW(check("a,b\n\"x\"\"y\",2\n")); // an escaped quote

  EXPECT_THROW(check("a,b,c\n1,2\n"), std::runtime_error); // short row
  EXPECT_THROW(check("a,b\n1,2,3\n"), std::runtime_error); // long row
  EXPECT_THROW(check("one column\nand another\n"), std::runtime_error);
  EXPECT_THROW(check(""), std::runtime_error);
}

/// An unterminated quote used to reach EOF and emit the partial field as a
/// record, so text that happens to hold an odd number of quotes and a
/// consistent comma count was classified as csv.
TEST(CsvFile, a_quoted_field_must_be_terminated) {
  const auto check = [](const std::string &content) {
    internal::csv::CsvFile(std::make_shared<internal::text::TextFile>(
        File::from_memory(content).impl()));
  };

  EXPECT_THROW(check("a,b\n1,\"2,3"), std::runtime_error);
  EXPECT_THROW(check("a,b\n\"1,2\n"), std::runtime_error);
  EXPECT_THROW(check("a,b\n1,\"2"), std::runtime_error);
}
