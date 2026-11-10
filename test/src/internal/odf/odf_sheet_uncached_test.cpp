#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>
#include <odr/odr.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

using namespace odr;

namespace {

/// A flat spreadsheet whose one row holds @p cells, with a data style N4 of
/// two decimals and grouping in German and a cell style ce1 using it.
std::string sheet_of(const std::string &cells) {
  return R"(<?xml version="1.0" encoding="UTF-8"?>)"
         R"(<office:document office:mimetype=")"
         R"(application/vnd.oasis.opendocument.spreadsheet">)"
         R"(<office:automatic-styles>)"
         R"(<number:number-style style:name="N4" number:language="de")"
         R"( number:country="DE"><number:number number:decimal-places="2")"
         R"( number:min-integer-digits="1" number:grouping="true"/>)"
         R"(</number:number-style>)"
         R"(<number:date-style style:name="N36" number:language="de")"
         R"( number:country="DE"><number:day number:style="long"/>)"
         R"(<number:text>.</number:text><number:month number:style="long"/>)"
         R"(<number:text>.</number:text><number:year number:style="long"/>)"
         R"(</number:date-style>)"
         R"(<style:style style:name="ce1" style:family="table-cell")"
         R"( style:data-style-name="N4"/>)"
         R"(<style:style style:name="ce2" style:family="table-cell")"
         R"( style:data-style-name="N36"/>)"
         R"(</office:automatic-styles><office:body><office:spreadsheet>)"
         R"(<table:table table:name="Sheet1"><table:table-row>)" +
         cells +
         R"(</table:table-row></table:table></office:spreadsheet>)"
         R"(</office:body></office:document>)";
}

/// What `SheetCell::value` states as the text of each cell of the row.
std::vector<std::optional<std::string>> texts_of(const std::string &source,
                                                 const std::uint32_t columns) {
  const Document document =
      open(File::from_memory(source)).as_document_file().document();
  const Sheet sheet = document.root_element().first_child().as_sheet();
  std::vector<std::optional<std::string>> result;
  for (std::uint32_t column = 0; column < columns; ++column) {
    const CellValue value = sheet.cell(column, 0).value();
    result.push_back(value.has_text() ? std::optional(value.text())
                                      : std::nullopt);
  }
  return result;
}

std::string render(const std::string &source) {
  const HtmlConfig config(
      (std::filesystem::current_path() / "odf_sheet_uncached").string());
  std::ostringstream out;
  html::translate(open(File::from_memory(source)), config)
      .list_views()
      .at(0)
      .write_html(out);
  return out.str();
}

std::size_t count(const std::string &haystack, const std::string &needle) {
  std::size_t result = 0;
  for (std::size_t at = haystack.find(needle); at != std::string::npos;
       at = haystack.find(needle, at + needle.size())) {
    ++result;
  }
  return result;
}

/// A row of sums as LibreOffice 4.2 saved it: the value, but no text.
constexpr const char *uncached_sums =
    R"(<table:table-cell table:style-name="ce1" table:formula="of:=1+1")"
    R"( office:value-type="float" office:value="55281.25"><text:p/>)"
    R"(</table:table-cell>)"
    R"(<table:table-cell office:value-type="float" office:value="474.6">)"
    R"(<text:p/></table:table-cell>)"
    R"(<table:table-cell office:value-type="string"><text:p/>)"
    R"(</table:table-cell>)";

} // namespace

TEST(OdfSheetUncached, a_number_without_text_shows_its_value) {
  // in its data style and language, else as General; the string cell shows
  // its own text, which is none
  EXPECT_EQ(
      texts_of(sheet_of(uncached_sums), 3),
      (std::vector<std::optional<std::string>>{"55.281,25", "474.6", ""}));

  const std::string html = render(sheet_of(uncached_sums));
  EXPECT_EQ(count(html, "55.281,25"), 1U);
  EXPECT_EQ(count(html, "474.6<"), 1U);
}

TEST(OdfSheetUncached, a_date_time_or_boolean_without_text_shows_its_value) {
  EXPECT_EQ(
      texts_of(sheet_of(R"(<table:table-cell table:style-name="ce2")"
                        R"( office:value-type="date")"
                        R"( office:date-value="2015-03-17"/>)"
                        R"(<table:table-cell office:value-type="boolean")"
                        R"( office:boolean-value="true"/>)"
                        R"(<table:table-cell office:value-type="boolean")"
                        R"( office:boolean-value="false"/>)"),
               3),
      (std::vector<std::optional<std::string>>{"17.03.2015", "TRUE", "FALSE"}));
}

TEST(OdfSheetUncached, a_cached_text_is_shown_as_the_file_has_it) {
  const std::string stale =
      sheet_of(R"(<table:table-cell table:style-name="ce1")"
               R"( office:value-type="float" office:value="55281.25">)"
               R"(<text:p>55281.25</text:p></table:table-cell>)");
  EXPECT_EQ(texts_of(stale, 1)[0], "55281.25");
  EXPECT_EQ(count(render(stale), "55.281,25"), 0U);
}

TEST(OdfSheetUncached, a_comment_is_no_text_of_the_cell) {
  EXPECT_EQ(texts_of(sheet_of(R"(<table:table-cell office:value-type="float")"
                              R"( office:value="7"><office:annotation>)"
                              R"(<text:p>checked</text:p></office:annotation>)"
                              R"(</table:table-cell>)"),
                     1)[0],
            "7");
}

TEST(OdfSheetUncached, a_frame_anchored_in_the_cell_is_no_text_of_it) {
  // the value shows beside the frame, which keeps its own text
  const std::string source =
      sheet_of(R"(<table:table-cell office:value-type="float")"
               R"( office:value="7"><draw:frame svg:width="2cm")"
               R"( svg:height="1cm"><draw:text-box><text:p>Note</text:p>)"
               R"(</draw:text-box></draw:frame><text:p/></table:table-cell>)");
  EXPECT_EQ(texts_of(source, 1)[0], "7");

  const std::string html = render(source);
  EXPECT_EQ(count(html, ">7<"), 1U);
  EXPECT_EQ(count(html, ">Note<"), 1U);
}

TEST(OdfSheetUncached, a_value_without_text_is_content_of_the_sheet) {
  // a column of uncached sums at the edge stays in the sheet
  const Document document =
      open(File::from_memory(
               sheet_of(R"(<table:table-cell office:value-type="string">)"
                        R"(<text:p>sum</text:p></table:table-cell>)"
                        R"(<table:table-cell office:value-type="float")"
                        R"( office:value="3"/>)")))
          .as_document_file()
          .document();
  const Sheet sheet = document.root_element().first_child().as_sheet();
  EXPECT_EQ(sheet.content(std::nullopt).columns, 2U);
}

TEST(OdfSheetUncached, a_text_cell_keeps_its_paragraphs) {
  const std::string html =
      render(sheet_of(R"(<table:table-cell office:value-type="string">)"
                      R"(<text:p>Line 1</text:p><text:p>Line 2</text:p>)"
                      R"(</table:table-cell>)"));
  EXPECT_EQ(count(html, ">Line 1"), 1U);
  EXPECT_EQ(count(html, "<x-p"), 2U);
}
