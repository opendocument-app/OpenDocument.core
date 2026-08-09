#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/document_path.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>
#include <odr/table_dimension.hpp>

#include <test_util.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <odr/internal/csv/csv_file.hpp>
#include <odr/internal/csv/csv_util.hpp>

#include <memory>
#include <sstream>
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

TEST(CsvOptions, detection_fills_in_what_was_not_given) {
  const CsvFile file =
      CsvFile::from_file(File::from_memory("a;b\n1;2\n"), CsvOptions{});

  const CsvOptions options = file.options();
  EXPECT_EQ(options.separator, ';');
  EXPECT_EQ(options.quote, '"');
  EXPECT_EQ(options.encoding, TextEncoding::utf8);
}

/// Detection is a guess; a caller who knows better does not have to argue with
/// it, and nothing is refused for disagreeing.
TEST(CsvOptions, a_given_separator_is_taken_as_given) {
  const std::string content = "a|b\n1|2\n";

  EXPECT_EQ(
      CsvFile::from_file(File::from_memory(content), {}).options().separator,
      '|');
  EXPECT_EQ(CsvFile::from_file(File::from_memory(content), {.separator = ','})
                .options()
                .separator,
            ',');
}

/// One column is no evidence of a csv, but it is a perfectly good csv once
/// someone says so.
TEST(CsvOptions, a_declared_separator_makes_anything_readable) {
  EXPECT_THROW((void)CsvFile::from_file(File::from_memory("a\nb\nc\n"), {}),
               NoCsvFile);
  EXPECT_NO_THROW((void)CsvFile::from_file(File::from_memory("a\nb\nc\n"),
                                           {.separator = ','}));
  // and so is prose, and an empty file
  EXPECT_NO_THROW((void)CsvFile::from_file(
      File::from_memory("lorem ipsum dolor\nsit amet\n"), {.separator = ','}));
  EXPECT_NO_THROW(
      (void)CsvFile::from_file(File::from_memory(""), {.separator = ','}));
}

TEST(CsvOptions, an_incoherent_dialect_is_a_caller_mistake) {
  EXPECT_THROW((void)CsvFile::from_file(File::from_memory("a,b\n"),
                                        {.separator = '"', .quote = '"'}),
               std::invalid_argument);
  EXPECT_THROW(
      (void)CsvFile::from_file(File::from_memory("a,b\n"), {.separator = '\n'}),
      std::invalid_argument);
}

TEST(CsvOptions, a_given_encoding_skips_detection) {
  // latin-1 bytes that are not valid utf-8; detection would not name them
  const File file = File::from_memory("caf\xe9,x\nb,y\n");

  EXPECT_EQ(CsvFile::from_file(file, {.encoding = TextEncoding::iso_8859_1})
                .options()
                .encoding,
            TextEncoding::iso_8859_1);
}

TEST(CsvOptions, with_options_derives_another_handle) {
  const CsvFile file =
      CsvFile::from_file(File::from_memory("a;b\n1;2\n"), CsvOptions{});
  const CsvFile other = file.with_options({.separator = ','});

  EXPECT_EQ(file.options().separator, ';');
  EXPECT_EQ(other.options().separator, ',');
}

TEST(CsvOptions, a_decoded_csv_is_reachable_as_one) {
  const File file(
      TestData::test_file_path("odr-public/csv/file_example_ODS_5000.csv"));
  const DecodedFile decoded(file, FileType::comma_separated_values);

  EXPECT_TRUE(decoded.is_csv_file());
  EXPECT_EQ(decoded.as_csv_file().options().separator, ',');
}

TEST(CsvDocument, a_csv_is_a_one_sheet_spreadsheet) {
  const CsvFile file = CsvFile::from_file(
      File::from_memory("a,b,c\n1,2,3\n4,5,6\n"), CsvOptions{});

  const Document document = file.document();
  EXPECT_EQ(document.document_type(), DocumentType::spreadsheet);
  const Sheet sheet = (*document.root_element().children().begin()).as_sheet();

  EXPECT_EQ(sheet.dimensions().rows, 3u);
  EXPECT_EQ(sheet.dimensions().columns, 3u);
  EXPECT_EQ((*sheet.cell(0, 0).children().begin()).as_text().content(), "a");
  EXPECT_EQ((*sheet.cell(2, 2).children().begin()).as_text().content(), "6");
}

/// The sheet is rectangular even where the file is not: a short row pads, a
/// long one widens.
TEST(CsvDocument, ragged_rows_become_a_rectangle) {
  const CsvFile file = CsvFile::from_file(File::from_memory("a,b\n1,2,3\n4\n"),
                                          CsvOptions{.separator = ','});

  const Document document = file.document();
  const Sheet sheet = (*document.root_element().children().begin()).as_sheet();

  EXPECT_EQ(sheet.dimensions().rows, 3u);
  EXPECT_EQ(sheet.dimensions().columns, 3u);
  EXPECT_EQ((*sheet.cell(2, 0).children().begin()).as_text().content(), "");
  EXPECT_EQ((*sheet.cell(2, 1).children().begin()).as_text().content(), "3");
}

TEST(CsvDocument, the_separator_directive_is_not_data) {
  const CsvFile file =
      CsvFile::from_file(File::from_memory("sep=;\na;b\n1;2\n"), CsvOptions{});

  const Document document = file.document();
  const Sheet sheet = (*document.root_element().children().begin()).as_sheet();

  EXPECT_EQ(sheet.dimensions().rows, 2u);
  EXPECT_EQ((*sheet.cell(0, 0).children().begin()).as_text().content(), "a");
}

/// Nothing can read those bytes, so nothing can probe them either — the
/// separator has to be declared, and there is still no document at the end.
TEST(CsvDocument, an_undecodable_encoding_has_no_document) {
  const File bytes = File::from_memory("a,b\n1,2\n");

  EXPECT_THROW(
      (void)CsvFile::from_file(bytes, {.encoding = TextEncoding::shift_jis}),
      NoCsvFile);

  const CsvFile file = CsvFile::from_file(
      bytes, {.encoding = TextEncoding::shift_jis, .separator = ','});
  EXPECT_FALSE(file.is_decodable());
  EXPECT_THROW((void)file.document(), UnsupportedTextEncoding);
}

TEST(CsvDocument, renders_as_a_table) {
  const CsvFile file =
      CsvFile::from_file(File::from_memory("a,b\n1,2\n"), CsvOptions{});

  const HtmlService service =
      html::translate(file.document(), "", HtmlConfig());
  std::ostringstream out;
  service.list_views().back().write_html(out);

  EXPECT_THAT(out.str(), testing::HasSubstr("<table"));
  EXPECT_THAT(out.str(), testing::HasSubstr("<td"));
  EXPECT_THAT(out.str(), testing::HasSubstr(">a<"));
  EXPECT_THAT(out.str(), testing::HasSubstr(">2<"));
}

TEST(CsvNumbers, a_number_is_a_number) {
  EXPECT_TRUE(csv::is_number("0"));
  EXPECT_TRUE(csv::is_number("42"));
  EXPECT_TRUE(csv::is_number("-42"));
  EXPECT_TRUE(csv::is_number("+42"));
  EXPECT_TRUE(csv::is_number("3.14"));
  EXPECT_TRUE(csv::is_number("0.5"));
  EXPECT_TRUE(csv::is_number("-0.5"));
  EXPECT_TRUE(csv::is_number("1e9"));
  EXPECT_TRUE(csv::is_number("1.5E-3"));
  EXPECT_TRUE(csv::is_number("  42  "));

  EXPECT_FALSE(csv::is_number(""));
  EXPECT_FALSE(csv::is_number("   "));
  EXPECT_FALSE(csv::is_number("abc"));
  EXPECT_FALSE(csv::is_number("42abc"));
  EXPECT_FALSE(csv::is_number("4 2"));
  EXPECT_FALSE(csv::is_number("."));
  EXPECT_FALSE(csv::is_number("1."));
  EXPECT_FALSE(csv::is_number("1e"));
  EXPECT_FALSE(csv::is_number("-"));
}

/// A leading zero is what tells a code from a quantity, and a thousands
/// separator does not say which side of the Atlantic wrote it.
TEST(CsvNumbers, an_identifier_is_not_a_number) {
  EXPECT_FALSE(csv::is_number("007"));
  EXPECT_FALSE(csv::is_number("0123456789012"));
  EXPECT_FALSE(csv::is_number("1,234"));
  EXPECT_FALSE(csv::is_number("1.234,56"));
  EXPECT_FALSE(csv::is_number("1,234.56"));
  // dates are left alone entirely
  EXPECT_FALSE(csv::is_number("2026-08-09"));
  EXPECT_FALSE(csv::is_number("03/04/2026"));
}

namespace {

ValueType value_type_at(const std::string &content, const std::uint32_t column,
                        const std::uint32_t row) {
  const CsvFile file = CsvFile::from_file(File::from_memory(content),
                                          CsvOptions{.separator = ','});
  const Document document = file.document();
  const Sheet sheet = (*document.root_element().children().begin()).as_sheet();
  return sheet.cell(column, row).value_type();
}

} // namespace

TEST(CsvValueType, a_numeric_column_is_a_number) {
  const std::string content = "name,age\nx,42\ny,7\n";

  EXPECT_EQ(value_type_at(content, 1, 1), ValueType::float_number);
  EXPECT_EQ(value_type_at(content, 1, 2), ValueType::float_number);
  EXPECT_EQ(value_type_at(content, 0, 1), ValueType::string);
  // the header of a numeric column is still a name
  EXPECT_EQ(value_type_at(content, 1, 0), ValueType::string);
}

/// A reader compares down a column, so one number in a column of prose is not
/// a quantity.
TEST(CsvValueType, a_lone_number_in_a_text_column_is_not) {
  EXPECT_EQ(value_type_at("a,b\nx,note\ny,42\n", 1, 2), ValueType::string);
}

TEST(CsvValueType, an_empty_cell_does_not_break_a_numeric_column) {
  const std::string content = "a,b\nx,1\ny,\nz,3\n";
  EXPECT_EQ(value_type_at(content, 1, 1), ValueType::float_number);
  EXPECT_EQ(value_type_at(content, 1, 2), ValueType::string);
  EXPECT_EQ(value_type_at(content, 1, 3), ValueType::float_number);
}

TEST(CsvValueType, a_column_of_codes_stays_text) {
  EXPECT_EQ(value_type_at("a,code\nx,007\ny,008\n", 1, 1), ValueType::string);
}

/// A column only one record reaches is padding everywhere else, and padding is
/// not a value: the inference sees the one field, not the empty rectangle
/// around it.
TEST(CsvValueType, a_column_one_wide_record_opened_holds_one_value) {
  const std::string content = "a\nb\nc,1\nd\n";

  EXPECT_EQ(value_type_at(content, 1, 2), ValueType::float_number);
  EXPECT_EQ(value_type_at(content, 1, 1), ValueType::string);
  EXPECT_EQ(value_type_at(content, 1, 3), ValueType::string);
}

/// Cells are not reachable by walking, so the generic path machinery has to
/// get at them the other way — through `sheet_cell`.
TEST(CsvDocument, a_cell_path_round_trips) {
  const CsvFile file =
      CsvFile::from_file(File::from_memory("a,b\n1,2\n3,4\n"), CsvOptions{});
  const Document document = file.document();
  const Sheet sheet = (*document.root_element().children().begin()).as_sheet();

  const SheetCell cell = sheet.cell(1, 2);
  const DocumentPath path = cell.document_path();

  const Element found = document.root_element().navigate_path(path);
  EXPECT_EQ(found.type(), ElementType::sheet_cell);
  EXPECT_EQ((*found.as_sheet_cell().children().begin()).as_text().content(),
            "4");
}

/// The whole point: a csv handed to the renderer comes out as a table, not a
/// line list.
TEST(CsvDocument, translating_the_decoded_file_yields_a_table) {
  const File bytes = File::from_memory("a,b\n1,2\n");
  const DecodedFile decoded(bytes, FileType::comma_separated_values);

  // a csv stays a text file and is rendered as a table anyway
  EXPECT_TRUE(decoded.is_text_file());

  const HtmlService service = html::translate(decoded, HtmlConfig());
  std::ostringstream out;
  service.list_views().back().write_html(out);

  EXPECT_THAT(out.str(), testing::HasSubstr("<table"));
  EXPECT_THAT(out.str(), testing::HasSubstr(">a<"));
}
