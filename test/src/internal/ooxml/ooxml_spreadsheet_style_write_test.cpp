#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/filesystem.hpp>
#include <odr/odr.hpp>
#include <odr/style.hpp>

#include <odr/internal/zip/zip_util.hpp>

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

TEST(OoxmlSpreadsheetStyleWrite, align_null_is_general_again) {
  const Document document = decode(workbook(one_string));

  document.edit(R"({"version": 2, "ops": [)"
                R"({"op": "setCellStyle", "sheet": 0, "column": 0, "row": 0,)"
                R"( "style": {"align": "right"}},)"
                R"({"op": "setCellStyle", "sheet": 0, "column": 0, "row": 0,)"
                R"( "style": {"align": null}}]})");

  EXPECT_EQ(first_sheet(reopened(document)).cell_style(0, 0).horizontal_align,
            std::nullopt);
  EXPECT_EQ(count(styles_of(document), R"(horizontal="general")"), 1);
}

/// Excel shows a line break only in a cell that wraps, so a write of several
/// lines turns wrapping on, as LibreOffice's export does.
TEST(OoxmlSpreadsheetStyleWrite, a_line_break_makes_the_cell_wrap) {
  const Document document = decode(workbook(one_string));

  first_sheet(document).set_cell(0, 0, CellValue("a\nb"));
  first_sheet(document).set_cell(1, 0, CellValue("c"));

  const Document saved = reopened(document);
  const Sheet sheet = first_sheet(saved);
  EXPECT_EQ(sheet.cell(0, 0).first_child().as_text().content(), "a\nb");
  EXPECT_EQ(sheet.cell_style(0, 0).wrap_text, true);
  EXPECT_NE(sheet.cell_style(1, 0).wrap_text, true);
}

namespace {

/// `A1` red on its own, `B1` red through column B, and `A3`.
Document rows_and_columns() {
  return decode(workbook(
      R"(<row r="1"><c r="A1" s="1" t="inlineStr"><is><t>a</t></is></c>)"
      R"(<c r="B1" t="inlineStr"><is><t>b</t></is></c></row>)"
      R"(<row r="3"><c r="A3" t="inlineStr"><is><t>x</t></is></c></row>)",
      "", "", "", R"(<cols><col min="2" max="2" width="9" style="1"/></cols>)",
      red_styles));
}

std::string sheet_of(const Document &document) {
  std::ostringstream xml;
  xml << reopened(document)
             .as_filesystem()
             .open("/xl/worksheets/sheet1.xml")
             .stream()
             ->rdbuf();
  return xml.str();
}

} // namespace

TEST(OoxmlSpreadsheetStyleWrite, a_row_style_keeps_what_each_cell_showed) {
  const Document document = rows_and_columns();

  first_sheet(document).set_row_style(0, {}, bold());

  const Document saved = reopened(document);
  const Sheet sheet = first_sheet(saved);
  EXPECT_EQ(fill_at(sheet, 0, 0), 0xff0000u);
  EXPECT_EQ(fill_at(sheet, 1, 0), 0xff0000u);
  EXPECT_EQ(text_style_at(sheet, 0).font_weight, FontWeight::bold);
  EXPECT_EQ(text_style_at(sheet, 1).font_weight, FontWeight::bold);
  EXPECT_NE(sheet_of(document).find(R"(customFormat="1")"), std::string::npos);
}

TEST(OoxmlSpreadsheetStyleWrite, a_row_style_reaches_past_the_cells) {
  const Document document = rows_and_columns();

  first_sheet(document).set_row_style(1, fill(0x00ff00_rgb), {});

  const Document saved = reopened(document);
  const Sheet sheet = first_sheet(saved);
  EXPECT_EQ(fill_at(sheet, 0, 1), 0x00ff00u);
  EXPECT_EQ(fill_at(sheet, 1, 1), 0x00ff00u);
  EXPECT_EQ(fill_at(sheet, 5, 1), 0x00ff00u);
  EXPECT_EQ(fill_at(sheet, 0, 2), std::nullopt);
  EXPECT_EQ(fill_at(sheet, 1, 2), 0xff0000u);
}

/// A row's format hides its column's where the row states no `c`, so the
/// crossing gets one.
TEST(OoxmlSpreadsheetStyleWrite, a_row_style_keeps_a_styled_column) {
  const Document document = rows_and_columns();

  first_sheet(document).set_row_style(1, {}, bold());

  const Document saved = reopened(document);
  const Sheet sheet = first_sheet(saved);
  EXPECT_EQ(fill_at(sheet, 1, 1), 0xff0000u);
  EXPECT_EQ(fill_at(sheet, 0, 1), std::nullopt);
}

TEST(OoxmlSpreadsheetStyleWrite, a_column_style_reaches_every_row) {
  const Document document = rows_and_columns();

  first_sheet(document).set_column_style(0, fill(0x00ff00_rgb), {});
  first_sheet(document).set_column_style(1, {}, bold());

  const Document saved = reopened(document);
  const Sheet sheet = first_sheet(saved);
  EXPECT_EQ(fill_at(sheet, 0, 0), 0x00ff00u);
  EXPECT_EQ(fill_at(sheet, 0, 2), 0x00ff00u);
  EXPECT_EQ(fill_at(sheet, 0, 100), 0x00ff00u);
  EXPECT_EQ(fill_at(sheet, 1, 100), 0xff0000u);
  EXPECT_EQ(text_style_at(sheet, 1).font_weight, FontWeight::bold);
}

TEST(OoxmlSpreadsheetStyleWrite, a_column_without_a_col_gets_one_of_its_own) {
  const Document document = rows_and_columns();

  first_sheet(document).set_row_style(0, {}, bold());
  first_sheet(document).set_column_style(3, fill(0x00ff00_rgb), {});

  const Document saved = reopened(document);
  const Sheet sheet = first_sheet(saved);
  EXPECT_EQ(fill_at(sheet, 3, 50), 0x00ff00u);
  EXPECT_EQ(fill_at(sheet, 2, 50), std::nullopt);
  // row 1 formats itself, so its crossing states a `c`
  EXPECT_EQ(fill_at(sheet, 3, 0), 0x00ff00u);
  EXPECT_NE(sheet_of(document).find(R"(<col min="2" max="2" width="9")"),
            std::string::npos);
  EXPECT_NE(sheet_of(document).find(R"(width="8.43" min="4" max="4")"),
            std::string::npos);
}

TEST(OoxmlSpreadsheetStyleWrite, a_col_of_several_columns_is_cut) {
  const Document document = decode(workbook(
      one_string, "", "", "",
      R"(<cols><col min="1" max="5" width="12" customWidth="1"/></cols>)"));

  first_sheet(document).set_column_style(2, fill(0x00ff00_rgb), {});

  const std::string xml = sheet_of(document);
  EXPECT_NE(xml.find(R"(<col min="1" max="2" width="12" customWidth="1"/>)"),
            std::string::npos)
      << xml;
  EXPECT_NE(xml.find(R"(<col min="4" max="5" width="12" customWidth="1"/>)"),
            std::string::npos);
  const Document saved = reopened(document);
  const Sheet sheet = first_sheet(saved);
  EXPECT_EQ(fill_at(sheet, 2, 9), 0x00ff00u);
  EXPECT_EQ(fill_at(sheet, 1, 9), std::nullopt);
  EXPECT_EQ(fill_at(sheet, 3, 9), std::nullopt);
}

TEST(OoxmlSpreadsheetStyleWrite, the_ops_name_a_row_and_a_column) {
  const Document document = rows_and_columns();

  document.edit(R"({"version": 2, "ops": [)"
                R"({"op": "setRowStyle", "sheet": 0, "row": 2,)"
                R"( "style": {"fill": "#00ff00"}},)"
                R"({"op": "setColumnStyle", "sheet": 0, "column": 4,)"
                R"( "style": {"fill": "#0000ff"}}]})");

  const Document saved = reopened(document);
  const Sheet sheet = first_sheet(saved);
  EXPECT_EQ(fill_at(sheet, 0, 2), 0x00ff00u);
  EXPECT_EQ(fill_at(sheet, 4, 7), 0x0000ffu);
}

/// LibreOffice reads a `c` without `s` through its row and its column, for
/// the text as much as for the fill.
TEST(OoxmlSpreadsheetStyleWrite, a_cell_without_s_shows_its_row_and_column) {
  const std::string bold_styles =
      R"(<fonts count="2"><font><sz val="11"/><name val="Calibri"/></font>)"
      R"(<font><b/><sz val="11"/><name val="Calibri"/></font></fonts>)"
      R"(<fills count="2"><fill><patternFill patternType="none"/></fill>)"
      R"(<fill><patternFill patternType="gray125"/></fill></fills>)"
      R"(<borders count="1"><border/></borders>)"
      R"(<cellStyleXfs count="1"><xf numFmtId="0" fontId="0" fillId="0" borderId="0"/></cellStyleXfs>)"
      R"(<cellXfs count="2"><xf numFmtId="0" fontId="0" fillId="0" borderId="0" xfId="0"/>)"
      R"(<xf numFmtId="0" fontId="1" fillId="0" borderId="0" xfId="0" applyFont="1"/>)"
      R"(</cellXfs>)";
  const Document document = decode(workbook(
      R"(<row r="1" s="1" customFormat="1">)"
      R"(<c r="A1" t="inlineStr"><is><t>a</t></is></c></row>)"
      R"(<row r="2"><c r="B2" t="inlineStr"><is><t>b</t></is></c>)"
      R"(<c r="C2" t="inlineStr"><is><t>c</t></is></c></row>)",
      "", "", "", R"(<cols><col min="2" max="2" width="9" style="1"/></cols>)",
      bold_styles));
  const Sheet sheet = first_sheet(document);

  EXPECT_EQ(sheet.cell(0, 0).first_child().as_text().style().font_weight,
            FontWeight::bold);
  EXPECT_EQ(sheet.cell(1, 1).first_child().as_text().style().font_weight,
            FontWeight::bold);
  EXPECT_NE(sheet.cell(2, 1).first_child().as_text().style().font_weight,
            FontWeight::bold);
}

TEST(OoxmlSpreadsheetStyleWrite, relocated_styles_and_strings_survive_edits) {
  const internal::zip::ZipArchive source(
      std::make_shared<internal::zip::util::Archive>(
          workbook(R"(<row r="1"><c r="A1" t="s" s="1"><v>1</v></c></row>)", "",
                   R"(<si><t>unused</t></si><extLst/><si><t>shared</t></si>)",
                   "", "", red_styles)));
  internal::zip::ZipArchive relocated;
  for (const auto &entry : source) {
    std::string path = entry.path().string();
    if (path == "xl/_rels/workbook.xml.rels") {
      insert(
          relocated, path,
          R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)"
          R"(<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/>)"
          R"(<Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="../parts/formats.xml"/>)"
          R"(<Relationship Id="rId3" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/sharedStrings" Target="/parts/strings.xml"/>)"
          R"(</Relationships>)");
      continue;
    }
    if (path == "xl/styles.xml") {
      path = "parts/formats.xml";
    } else if (path == "xl/sharedStrings.xml") {
      path = "parts/strings.xml";
    }
    relocated.insert_file(relocated.end(), internal::RelPath(path),
                          entry.file());
  }
  std::ostringstream bytes;
  relocated.save(bytes);
  const Document document =
      decode(std::make_shared<internal::MemoryFile>(bytes.str()));
  const Sheet sheet = first_sheet(document);
  EXPECT_EQ(sheet.cell(0, 0).value().text(), "shared");
  EXPECT_EQ(fill_at(sheet, 0, 0), 0xff0000u);
  sheet.set_cell_style(0, 0, fill(0x00ff00_rgb), bold());

  const Document saved = reopened(document);
  EXPECT_EQ(first_sheet(saved).cell(0, 0).value().text(), "shared");
  EXPECT_EQ(fill_at(first_sheet(saved), 0, 0), 0x00ff00u);
  EXPECT_EQ(text_style_at(first_sheet(saved), 0).font_weight, FontWeight::bold);
  EXPECT_TRUE(saved.as_filesystem().is_file("/parts/formats.xml"));
  EXPECT_FALSE(saved.as_filesystem().exists("/xl/styles.xml"));
}

TEST(OoxmlSpreadsheetStyleWrite,
     a_missing_styles_part_can_be_created_and_saved) {
  const internal::zip::ZipArchive source(std::make_shared<
                                         internal::zip::util::Archive>(workbook(
      R"(<row r="1"><c r="A1" s="0" t="inlineStr"><is><t>a</t></is></c></row>)")));
  internal::zip::ZipArchive without_styles;
  for (const auto &entry : source) {
    if (entry.path().string() != "xl/styles.xml") {
      without_styles.insert_file(without_styles.end(), entry.path(),
                                 entry.file());
    }
  }
  std::ostringstream bytes;
  without_styles.save(bytes);
  const Document document =
      decode(std::make_shared<internal::MemoryFile>(bytes.str()));
  const Sheet sheet = first_sheet(document);
  EXPECT_EQ(sheet.cell(0, 0).value().text(), "a");
  EXPECT_EQ(fill_at(sheet, 0, 0), std::nullopt);
  EXPECT_EQ(first_sheet(reopened(document)).cell(0, 0).value().text(), "a");
  sheet.set_cell_style(0, 0, fill(0x00ff00_rgb), bold());
  const Document saved = reopened(document);
  EXPECT_EQ(fill_at(first_sheet(saved), 0, 0), 0x00ff00u);
  EXPECT_EQ(text_style_at(first_sheet(saved), 0).font_weight, FontWeight::bold);
  for (const char *path :
       {"/xl/_rels/workbook.xml.rels", "/[Content_Types].xml"}) {
    std::ostringstream xml;
    xml << saved.as_filesystem().open(path).stream()->rdbuf();
    EXPECT_NE(xml.str().find("styles.xml"), std::string::npos) << path;
  }
  EXPECT_EQ(fill_at(first_sheet(reopened(saved)), 0, 0), 0x00ff00u);
}
