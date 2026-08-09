#include <odr/exceptions.hpp>
#include <odr/file.hpp>

#include <test_util.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <odr/internal/csv/csv_file.hpp>
#include <odr/internal/csv/csv_util.hpp>

#include <memory>
#include <string>
#include <vector>

using namespace odr;
using namespace odr::internal;
using namespace odr::test;

namespace {

/// Detection over a whole file, which is what the probe sees for anything
/// shorter than its bound.
csv::Probe probe(const std::string &content) {
  return csv::probe(content, true);
}

std::vector<std::vector<std::string>> records(const std::string &content,
                                              const csv::Dialect dialect) {
  std::vector<std::vector<std::string>> result;
  csv::RecordReader reader(content, dialect);
  std::vector<std::string> fields;
  while (reader.read(fields)) {
    result.push_back(fields);
  }
  return result;
}

} // namespace

TEST(CsvFile, odt) {
  const File file(TestData::test_file_path("odr-public/odt/about.odt"));
  EXPECT_THROW(internal::csv::CsvFile(
                   std::make_shared<internal::text::TextFile>(file.impl())),
               odr::Exception);
}

TEST(CsvFile, txt) {
  const File file(TestData::test_file_path("odr-public/txt/lorem ipsum.txt"));
  EXPECT_THROW(internal::csv::CsvFile(
                   std::make_shared<internal::text::TextFile>(file.impl())),
               NoCsvFile);
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

TEST(CsvProbe, a_consistent_field_count_is_what_makes_it_a_csv) {
  EXPECT_TRUE(probe("a,b,c\n1,2,3\n").is_csv);
  EXPECT_TRUE(probe("a,b,c\n1,2,3").is_csv);        // no trailing newline
  EXPECT_TRUE(probe("a,b\r\n1,2\r\n").is_csv);      // crlf
  EXPECT_TRUE(probe("a,b\n\"x,y\",2\n").is_csv);    // a quoted separator
  EXPECT_TRUE(probe("a,b\n\"x\ny\",2\n").is_csv);   // a quoted newline
  EXPECT_TRUE(probe("a,b\n\"x\"\"y\",2\n").is_csv); // an escaped quote

  EXPECT_FALSE(probe("one column\nand another\n").is_csv);
  EXPECT_FALSE(probe("").is_csv);
}

/// One column is every line of prose ever written, so it is no evidence of a
/// csv — which says nothing about whether a one-column csv is legitimate.
TEST(CsvProbe, one_column_is_not_evidence) {
  EXPECT_FALSE(probe("a\nb\nc\n").is_csv);
  EXPECT_EQ(records("a\nb\nc\n", {}).size(), 3u);
}

/// A file that happens to hold an odd number of quotes and a consistent
/// separator count used to be classified as csv.
TEST(CsvProbe, a_dangling_quote_in_a_complete_file_is_evidence_against) {
  EXPECT_FALSE(probe("a,b\n1,\"2,3").is_csv);
  EXPECT_FALSE(probe("a,b\n\"1,2\n").is_csv);
  EXPECT_FALSE(probe("a,b\n1,\"2").is_csv);
}

/// The same text cut short says nothing — the sample ended mid-record, not the
/// file.
TEST(CsvProbe, a_dangling_quote_in_a_sample_is_not) {
  EXPECT_TRUE(csv::probe("a,b\n1,2\n3,4\n5,\"6", false).is_csv);
}

TEST(CsvProbe, the_separator_is_the_one_that_explains_the_file) {
  EXPECT_EQ(probe("a;b;c\n1;2;3\n").dialect.separator, ';');
  EXPECT_EQ(probe("a\tb\tc\n1\t2\t3\n").dialect.separator, '\t');
  EXPECT_EQ(probe("a|b|c\n1|2|3\n").dialect.separator, '|');
  EXPECT_EQ(probe("a,b,c\n1,2,3\n").dialect.separator, ',');
}

/// Commas inside fields of a semicolon-separated file must not win: they do
/// not produce a consistent count.
TEST(CsvProbe, a_separator_that_only_sometimes_appears_loses) {
  const csv::Probe result = probe("name;note\nx;a, b, c\ny;d\n");
  EXPECT_TRUE(result.is_csv);
  EXPECT_EQ(result.dialect.separator, ';');
  EXPECT_EQ(result.columns, 2u);
}

TEST(CsvProbe, excel_declares_its_separator) {
  const csv::Probe result = probe("sep=;\na;b\n1;2\n");
  EXPECT_TRUE(result.is_csv);
  EXPECT_TRUE(result.separator_directive);
  EXPECT_EQ(result.dialect.separator, ';');
  EXPECT_EQ(result.columns, 2u);
}

TEST(RecordReader, parses_rfc4180) {
  EXPECT_EQ(records("a,b\n1,2\n", {}),
            (std::vector<std::vector<std::string>>{{"a", "b"}, {"1", "2"}}));
  EXPECT_EQ(records("\"x,y\",2\n", {}),
            (std::vector<std::vector<std::string>>{{"x,y", "2"}}));
  EXPECT_EQ(records("\"x\ny\",2\n", {}),
            (std::vector<std::vector<std::string>>{{"x\ny", "2"}}));
  EXPECT_EQ(records("\"x\"\"y\",2\n", {}),
            (std::vector<std::vector<std::string>>{{"x\"y", "2"}}));
}

/// The parser judges nothing: a ragged file is a ragged file, and the sheet
/// pads it later.
TEST(RecordReader, ragged_records_come_out_as_they_are) {
  EXPECT_EQ(records("a,b,c\n1,2\n", {}), (std::vector<std::vector<std::string>>{
                                             {"a", "b", "c"}, {"1", "2"}}));
}

TEST(RecordReader, an_unterminated_quote_still_yields_its_field) {
  csv::RecordReader reader("a,\"b", csv::Dialect{});
  std::vector<std::string> fields;
  EXPECT_TRUE(reader.read(fields));
  EXPECT_EQ(fields, (std::vector<std::string>{"a", "b"}));
  EXPECT_TRUE(reader.unterminated());
}
