#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/filesystem.hpp>
#include <odr/odr.hpp>
#include <odr/style.hpp>

#include <internal/ooxml/ooxml_spreadsheet_test_util.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>

using namespace odr;
using namespace odr::test::ooxml;

namespace {

constexpr const char *one_string =
    R"(<row r="1"><c r="A1" t="inlineStr"><is><t>a</t></is></c></row>)";

/// Two cells sharing `xf` 1, which fills them red.
constexpr const char *two_red =
    R"(<row r="1">)"
    R"(<c r="A1" s="1" t="inlineStr"><is><t>a</t></is></c>)"
    R"(<c r="B1" s="1" t="inlineStr"><is><t>b</t></is></c>)"
    R"(</row>)";
constexpr const char *red_styles =
    R"(<fonts count="1"><font><sz val="11"/><name val="Calibri"/></font></fonts>)"
    R"(<fills count="3"><fill><patternFill patternType="none"/></fill>)"
    R"(<fill><patternFill patternType="gray125"/></fill>)"
    R"(<fill><patternFill patternType="solid"><fgColor rgb="FFFF0000"/>)"
    R"(</patternFill></fill></fills>)"
    R"(<borders count="1"><border/></borders>)"
    R"(<cellStyleXfs count="1"><xf numFmtId="0" fontId="0" fillId="0" borderId="0"/></cellStyleXfs>)"
    R"(<cellXfs count="2"><xf numFmtId="0" fontId="0" fillId="0" borderId="0" xfId="0"/>)"
    R"(<xf numFmtId="0" fontId="0" fillId="2" borderId="0" xfId="0" applyFill="1"/>)"
    R"(</cellXfs>)";

TableCellStyle fill(const Color color) {
  TableCellStyle style;
  style.background_color = color;
  return style;
}

TextStyle bold() {
  TextStyle style;
  style.font_weight = FontWeight::bold;
  return style;
}

std::optional<std::uint32_t> fill_at(const Sheet &sheet,
                                     const std::uint32_t column,
                                     const std::uint32_t row) {
  const std::optional<Color> color =
      sheet.cell_style(column, row).background_color;
  return color ? std::optional(color->rgb()) : std::nullopt;
}

/// The style the one run of the cell shows.
TextStyle text_style_at(const Sheet &sheet, const std::uint32_t column) {
  return sheet.cell(column, 0).first_child().as_text().style();
}

Document reopened(const Document &document) {
  std::ostringstream saved;
  document.save(saved);
  return open(File::from_memory(saved.str())).as_document_file().document();
}

std::string styles_of(const Document &document) {
  std::ostringstream xml;
  xml << reopened(document)
             .as_filesystem()
             .open("/xl/styles.xml")
             .stream()
             ->rdbuf();
  return xml.str();
}

std::size_t count(const std::string &text, const std::string &part) {
  std::size_t result = 0;
  for (std::size_t at = text.find(part); at != std::string::npos;
       at = text.find(part, at + 1)) {
    ++result;
  }
  return result;
}

} // namespace

TEST(OoxmlSpreadsheetStyleWrite,
     a_fill_survives_a_save_of_an_empty_style_sheet) {
  const Document document = decode(workbook(one_string));

  first_sheet(document).set_cell_style(0, 0, fill(0xffff00_rgb), {});

  EXPECT_EQ(fill_at(first_sheet(document), 0, 0), 0xffff00u);
  const Document saved = reopened(document);
  EXPECT_EQ(fill_at(first_sheet(saved), 0, 0), 0xffff00u);
  EXPECT_EQ(first_sheet(saved).cell(0, 0).value().text(), "a");
}

TEST(OoxmlSpreadsheetStyleWrite, a_font_takes_every_text_key) {
  const Document document = decode(workbook(one_string));
  TextStyle style;
  style.font_weight = FontWeight::bold;
  style.font_style = FontStyle::italic;
  style.font_underline = true;
  style.font_line_through = true;
  style.font_color = 0xcc0000_rgb;
  style.font_size = Measure("16pt");

  first_sheet(document).set_cell_style(0, 0, {}, style);

  const TextStyle read = text_style_at(first_sheet(reopened(document)), 0);
  EXPECT_EQ(read.font_weight, FontWeight::bold);
  EXPECT_EQ(read.font_style, FontStyle::italic);
  EXPECT_EQ(read.font_underline, true);
  EXPECT_EQ(read.font_line_through, true);
  ASSERT_TRUE(read.font_color.has_value());
  EXPECT_EQ(read.font_color->rgb(), 0xcc0000u);
  EXPECT_EQ(read.font_size, Measure("16pt"));
}

TEST(OoxmlSpreadsheetStyleWrite, a_format_shared_with_another_cell_is_copied) {
  const Document document =
      decode(workbook(two_red, "", "", "", "", red_styles));
  const Sheet sheet = first_sheet(document);

  sheet.set_cell_style(0, 0, {}, bold());

  EXPECT_EQ(text_style_at(sheet, 0).font_weight, FontWeight::bold);
  EXPECT_EQ(fill_at(sheet, 0, 0), 0xff0000u);
  EXPECT_NE(text_style_at(sheet, 1).font_weight, FontWeight::bold);
  EXPECT_EQ(fill_at(sheet, 1, 0), 0xff0000u);
}

TEST(OoxmlSpreadsheetStyleWrite, one_delta_on_one_base_is_one_format) {
  const Document document =
      decode(workbook(two_red, "", "", "", "", red_styles));
  const Sheet sheet = first_sheet(document);

  sheet.set_cell_style(0, 0, fill(0x00ff00_rgb), bold());
  sheet.set_cell_style(1, 0, fill(0x00ff00_rgb), bold());

  const std::string styles = styles_of(document);
  EXPECT_EQ(count(styles, "<xf "), 1 + 3);
  EXPECT_NE(styles.find(R"(<cellXfs count="3">)"), std::string::npos);
  EXPECT_EQ(count(styles, "<font>"), 2);
  EXPECT_EQ(count(styles, "<fill>"), 4);
}

TEST(OoxmlSpreadsheetStyleWrite, a_fill_taken_away_is_no_pattern) {
  const Document document =
      decode(workbook(two_red, "", "", "", "", red_styles));
  const Sheet sheet = first_sheet(document);

  sheet.set_cell_style(0, 0, fill(Color(0, 0, 0, 0)), {});

  EXPECT_EQ(fill_at(sheet, 0, 0), std::nullopt);
  EXPECT_EQ(fill_at(sheet, 1, 0), 0xff0000u);
}

TEST(OoxmlSpreadsheetStyleWrite, an_alignment_lands_in_the_format) {
  const Document document = decode(workbook(one_string));
  TableCellStyle right;
  right.horizontal_align = HorizontalAlign::right;

  first_sheet(document).set_cell_style(0, 0, right, {});

  EXPECT_EQ(first_sheet(reopened(document)).cell_style(0, 0).horizontal_align,
            HorizontalAlign::right);
}

TEST(OoxmlSpreadsheetStyleWrite, a_cell_the_file_does_not_state_is_made) {
  const Document document = decode(workbook(one_string));

  first_sheet(document).set_cell_style(2, 3, fill(0x0000ff_rgb), {});

  EXPECT_EQ(fill_at(first_sheet(reopened(document)), 2, 3), 0x0000ffu);
}

TEST(OoxmlSpreadsheetStyleWrite, a_cell_without_a_format_starts_from_its_row) {
  const Document document = decode(
      workbook(R"(<row r="1" s="1" customFormat="1"><c r="A1" t="inlineStr">)"
               R"(<is><t>a</t></is></c></row>)",
               "", "", "", "", red_styles));

  first_sheet(document).set_cell_style(0, 0, {}, bold());

  const Document saved = reopened(document);
  const Sheet sheet = first_sheet(saved);
  EXPECT_EQ(fill_at(sheet, 0, 0), 0xff0000u);
  EXPECT_EQ(text_style_at(sheet, 0).font_weight, FontWeight::bold);
}

TEST(OoxmlSpreadsheetStyleWrite, a_cell_starts_from_its_own_column_only) {
  const Document document = decode(
      workbook(one_string, "", "", "",
               R"(<cols><col min="3" max="3" style="1"/></cols>)", red_styles));

  first_sheet(document).set_cell_style(0, 0, {}, bold());
  first_sheet(document).set_cell_style(2, 0, {}, bold());

  const Document saved = reopened(document);
  const Sheet sheet = first_sheet(saved);
  EXPECT_EQ(fill_at(sheet, 0, 0), std::nullopt);
  EXPECT_EQ(fill_at(sheet, 2, 0), 0xff0000u);
}

TEST(OoxmlSpreadsheetStyleWrite, a_covered_cell_refuses) {
  const Document document = decode(workbook(
      R"(<row r="1"><c r="A1" t="inlineStr"><is><t>a</t></is></c><c r="B1"/></row>)",
      R"(<mergeCells count="1"><mergeCell ref="A1:B1"/></mergeCells>)"));

  EXPECT_THROW(
      first_sheet(document).set_cell_style(1, 0, fill(0xffff00_rgb), {}),
      UnsupportedOperation);
}

TEST(OoxmlSpreadsheetStyleWrite, the_op_carries_the_fill_and_the_text_keys) {
  const Document document = decode(workbook(one_string));

  document.edit(
      R"({"version": 2, "ops": [{"op": "setCellStyle", "sheet": 0,)"
      R"( "column": 0, "row": 0,)"
      R"( "style": {"fill": "#ffff00", "bold": true, "align": "center"}}]})");

  const Sheet sheet = first_sheet(document);
  EXPECT_EQ(fill_at(sheet, 0, 0), 0xffff00u);
  EXPECT_EQ(text_style_at(sheet, 0).font_weight, FontWeight::bold);
  EXPECT_EQ(sheet.cell_style(0, 0).horizontal_align, HorizontalAlign::center);
}
