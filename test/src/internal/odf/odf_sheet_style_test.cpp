#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/logger.hpp>
#include <odr/style.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/open_strategy.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <sstream>
#include <string>

using namespace odr;
using namespace odr::internal;

namespace {

/// A flat sheet whose single row holds @p cells, under @p automatic_styles
/// and @p styles.
std::string flat_sheet(const std::string &cells,
                       const std::string &automatic_styles = "",
                       const std::string &styles = "") {
  return R"(<?xml version="1.0" encoding="UTF-8"?>)"
         R"(<office:document office:mimetype=")"
         R"(application/vnd.oasis.opendocument.spreadsheet">)"
         R"(<office:styles>)" +
         styles + R"(</office:styles><office:automatic-styles>)" +
         automatic_styles +
         R"(</office:automatic-styles><office:body><office:spreadsheet>)"
         R"(<table:table table:name="s"><table:table-row>)" +
         cells +
         R"(</table:table-row></table:table>)"
         R"(</office:spreadsheet></office:body></office:document>)";
}

std::string string_cell(const std::string &text,
                        const std::string &style_name = "") {
  return R"(<table:table-cell office:value-type="string")" +
         (style_name.empty() ? std::string()
                             : R"( table:style-name=")" + style_name + R"(")") +
         R"(><text:p>)" + text + R"(</text:p></table:table-cell>)";
}

constexpr const char *red_cell_style =
    R"(<style:style style:name="ce1" style:family="table-cell">)"
    R"(<style:table-cell-properties fo:background-color="#ff0000"/>)"
    R"(</style:style>)";

Document document_of(const std::string &source) {
  return DecodedFile(
             open_strategy::open_file(std::make_shared<MemoryFile>(source), {},
                                      Logger::null()))
      .as_document_file()
      .document();
}

Sheet first_sheet(const Document &document) {
  return (*document.root_element().children().begin()).as_sheet();
}

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

/// The style the text of the cell's one run shows.
TextStyle text_style_of(const Sheet &sheet, const std::uint32_t column) {
  return sheet.cell(column, 0).first_child().first_child().as_text().style();
}

/// The fill of the cell as `0xRRGGBB`, none where it states none.
std::optional<std::uint32_t> fill_at(const Sheet &sheet,
                                     const std::uint32_t column,
                                     const std::uint32_t row) {
  const std::optional<Color> color =
      sheet.cell_style(column, row).background_color;
  return color ? std::optional(color->rgb()) : std::nullopt;
}

std::string saved(const Document &document) {
  std::ostringstream out;
  document.save(out);
  return out.str();
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

TEST(OdfSheetStyle, a_fill_lands_on_the_cell) {
  const Document document = document_of(flat_sheet(string_cell("a")));
  const Sheet sheet = first_sheet(document);

  sheet.set_cell_style(0, 0, fill(0xffff00_rgb), {});

  EXPECT_EQ(fill_at(sheet, 0, 0), 0xffff00u);
  EXPECT_EQ(sheet.cell(0, 0).value().text(), "a");
}

TEST(OdfSheetStyle, a_style_shared_with_another_cell_is_copied) {
  const Document document = document_of(flat_sheet(
      string_cell("a", "ce1") + string_cell("b", "ce1"), red_cell_style));
  const Sheet sheet = first_sheet(document);

  sheet.set_cell_style(0, 0, {}, bold());

  EXPECT_EQ(text_style_of(sheet, 0).font_weight, FontWeight::bold);
  EXPECT_EQ(fill_at(sheet, 0, 0), 0xff0000u);
  EXPECT_NE(text_style_of(sheet, 1).font_weight, FontWeight::bold);
  EXPECT_EQ(fill_at(sheet, 1, 0), 0xff0000u);
}

TEST(OdfSheetStyle, a_named_style_is_inherited_from) {
  const Document document = document_of(flat_sheet(
      string_cell("a", "Heading"), "",
      R"(<style:style style:name="Heading" style:family="table-cell">)"
      R"(<style:text-properties fo:font-weight="bold"/></style:style>)"));
  const Sheet sheet = first_sheet(document);

  sheet.set_cell_style(0, 0, fill(0x00ff00_rgb), {});

  EXPECT_EQ(text_style_of(sheet, 0).font_weight, FontWeight::bold);
  EXPECT_EQ(fill_at(sheet, 0, 0), 0x00ff00u);
  EXPECT_NE(saved(document).find(R"(style:parent-style-name="Heading")"),
            std::string::npos);
}

TEST(OdfSheetStyle, one_cell_of_a_repeated_run_takes_the_style_alone) {
  const Document document = document_of(
      flat_sheet(R"(<table:table-cell table:number-columns-repeated="3"/>)"));
  const Sheet sheet = first_sheet(document);

  sheet.set_cell_style(1, 0, fill(0x0000ff_rgb), {});

  EXPECT_EQ(fill_at(sheet, 0, 0), std::nullopt);
  EXPECT_EQ(fill_at(sheet, 1, 0), 0x0000ffu);
  EXPECT_EQ(fill_at(sheet, 2, 0), std::nullopt);
}

TEST(OdfSheetStyle, an_empty_cell_after_a_span_is_claimed_where_it_sits) {
  const Document document = document_of(flat_sheet(
      R"(<table:table-cell table:number-columns-spanned="2"><text:p>a</text:p>)"
      R"(</table:table-cell><table:covered-table-cell/>)"
      R"(<table:table-cell table:number-columns-repeated="2"/>)"));
  const Sheet sheet = first_sheet(document);

  sheet.set_cell_style(3, 0, fill(0x0000ff_rgb), {});
  sheet.set_cell(2, 0, CellValue("b"));

  EXPECT_EQ(fill_at(sheet, 2, 0), std::nullopt);
  EXPECT_EQ(fill_at(sheet, 3, 0), 0x0000ffu);
  EXPECT_EQ(sheet.cell(2, 0).value().text(), "b");
  EXPECT_EQ(sheet.cell(0, 0).value().text(), "a");
}

TEST(OdfSheetStyle, a_cell_past_the_sheet_is_made) {
  const Document document = document_of(flat_sheet(string_cell("a")));
  const Sheet sheet = first_sheet(document);

  sheet.set_cell_style(3, 2, fill(0x0000ff_rgb), {});

  EXPECT_EQ(fill_at(sheet, 3, 2), 0x0000ffu);
  const Document reopened = document_of(saved(document));
  EXPECT_EQ(fill_at(first_sheet(reopened), 3, 2), 0x0000ffu);
}

TEST(OdfSheetStyle, one_delta_on_one_base_is_one_style) {
  const Document document = document_of(flat_sheet(
      string_cell("a", "ce1") + string_cell("b", "ce1") + string_cell("c"),
      red_cell_style));
  const Sheet sheet = first_sheet(document);

  sheet.set_cell_style(0, 0, {}, bold());
  sheet.set_cell_style(1, 0, {}, bold());
  sheet.set_cell_style(2, 0, {}, bold());

  // `ce1`, one copy of it, and one style over no base
  EXPECT_EQ(count(saved(document), R"(style:family="table-cell")"), 3);
}

TEST(OdfSheetStyle, a_fill_taken_away_is_written_transparent) {
  const Document document =
      document_of(flat_sheet(string_cell("a", "ce1"), red_cell_style));
  const Sheet sheet = first_sheet(document);

  sheet.set_cell_style(0, 0, fill(Color(0, 0, 0, 0)), {});

  EXPECT_EQ(fill_at(sheet, 0, 0), std::nullopt);
  EXPECT_NE(saved(document).find(R"(fo:background-color="transparent")"),
            std::string::npos);
}

TEST(OdfSheetStyle, an_alignment_is_fixed_on_the_cell) {
  const Document document = document_of(flat_sheet(string_cell("a")));
  const Sheet sheet = first_sheet(document);

  TableCellStyle right;
  right.horizontal_align = HorizontalAlign::right;
  sheet.set_cell_style(0, 0, right, {});

  EXPECT_EQ(sheet.cell(0, 0).first_child().as_paragraph().style().text_align,
            TextAlign::right);
  const std::string xml = saved(document);
  EXPECT_NE(xml.find(R"(style:text-align-source="fix")"), std::string::npos);
  EXPECT_NE(xml.find(R"(fo:text-align="right")"), std::string::npos);
}

TEST(OdfSheetStyle, general_aligns_by_the_value_type_again) {
  const Document document = document_of(flat_sheet(
      string_cell("a", "Right") + string_cell("b"), "",
      R"(<style:style style:name="Right" style:family="table-cell">)"
      R"(<style:table-cell-properties style:text-align-source="fix"/>)"
      R"(<style:paragraph-properties fo:text-align="end"/></style:style>)"));
  const Sheet sheet = first_sheet(document);
  const auto align_at = [&sheet](const std::uint32_t column) {
    return sheet.cell(column, 0)
        .first_child()
        .as_paragraph()
        .style()
        .text_align;
  };

  TableCellStyle right;
  right.horizontal_align = HorizontalAlign::right;
  TableCellStyle general;
  general.horizontal_align = HorizontalAlign::general;
  sheet.set_cell_style(0, 0, general, {});
  sheet.set_cell_style(1, 0, right, {});
  sheet.set_cell_style(1, 0, general, {});

  EXPECT_EQ(align_at(0), std::nullopt);
  EXPECT_EQ(align_at(1), std::nullopt);
  const std::string xml = saved(document);
  EXPECT_EQ(count(xml, R"(style:text-align-source="value-type")"), 2);
  EXPECT_EQ(count(xml, R"(fo:text-align="right")"), 1);
}

TEST(OdfSheetStyle, value_type_drops_the_inherited_alignment_only) {
  const std::string parent =
      R"(<style:style style:name="Right" style:family="table-cell">)"
      R"(<style:paragraph-properties fo:text-align="end"/></style:style>)";
  const auto align_of = [&parent](const std::string &own) {
    const Document document = document_of(flat_sheet(
        string_cell("a", "ce1"),
        R"(<style:style style:name="ce1" style:family="table-cell")"
        R"( style:parent-style-name="Right">)"
        R"(<style:table-cell-properties style:text-align-source="value-type"/>)" +
            own + "</style:style>",
        parent));
    return first_sheet(document)
        .cell(0, 0)
        .first_child()
        .as_paragraph()
        .style()
        .text_align;
  };

  EXPECT_EQ(align_of(""), std::nullopt);
  EXPECT_EQ(align_of(R"(<style:paragraph-properties fo:text-align="center"/>)"),
            TextAlign::center);
}

TEST(OdfSheetStyle, a_formula_cell_takes_a_style) {
  const Document document = document_of(flat_sheet(
      R"(<table:table-cell table:formula="of:=1+1" office:value-type="float")"
      R"( office:value="2"><text:p>2</text:p></table:table-cell>)"));
  const Sheet sheet = first_sheet(document);

  sheet.set_cell_style(0, 0, fill(0xffff00_rgb), {});

  EXPECT_EQ(fill_at(sheet, 0, 0), 0xffff00u);
  EXPECT_EQ(sheet.cell(0, 0).value().formula(), "of:=1+1");
}

TEST(OdfSheetStyle, a_covered_cell_or_a_property_no_engine_writes_refuses) {
  const Document document = document_of(flat_sheet(string_cell("a")));
  const Sheet sheet = first_sheet(document);

  TableCellStyle wrap;
  wrap.wrap_text = true;
  EXPECT_THROW(sheet.set_cell_style(0, 0, wrap, {}), UnsupportedOperation);

  const Document merged = document_of(flat_sheet(
      R"(<table:table-cell table:number-columns-spanned="2"><text:p>a</text:p>)"
      R"(</table:table-cell><table:covered-table-cell/>)"));
  EXPECT_THROW(first_sheet(merged).set_cell_style(1, 0, fill(0xffff00_rgb), {}),
               UnsupportedOperation);
  EXPECT_THROW(first_sheet(merged).set_cell(1, 0, CellValue("x")),
               UnsupportedOperation);

  TextStyle highlight;
  highlight.background_color = 0xffff00_rgb;
  EXPECT_THROW(sheet.set_cell_style(0, 0, {}, highlight), UnsupportedOperation);
}

TEST(OdfSheetStyle, the_op_carries_the_fill_and_the_text_keys) {
  const Document document = document_of(flat_sheet(string_cell("a")));

  document.edit(
      R"({"version": 2, "ops": [{"op": "setCellStyle", "sheet": 0,)"
      R"( "column": 0, "row": 0,)"
      R"( "style": {"fill": "#ffff00", "bold": true, "align": "center"}}]})");

  const Sheet sheet = first_sheet(document);
  EXPECT_EQ(fill_at(sheet, 0, 0), 0xffff00u);
  EXPECT_EQ(text_style_of(sheet, 0).font_weight, FontWeight::bold);

  EXPECT_THROW(
      document.edit(
          R"({"version": 2, "ops": [{"op": "setCellStyle", "sheet": 0,)"
          R"( "column": 0, "row": 0, "style": {"highlight": "#ffff00"}}]})"),
      std::invalid_argument);
}

namespace {

/// Three columns, B defaulting to blue, and two rows: one stating `a` red,
/// `b` and nothing else, and one standing for three rows.
std::string rows_and_columns() {
  return R"(<?xml version="1.0" encoding="UTF-8"?>)"
         R"(<office:document office:mimetype=")"
         R"(application/vnd.oasis.opendocument.spreadsheet">)"
         R"(<office:automatic-styles>)" +
         std::string(red_cell_style) +
         R"(<style:style style:name="Blue" style:family="table-cell">)"
         R"(<style:table-cell-properties fo:background-color="#0000ff"/>)"
         R"(</style:style></office:automatic-styles>)"
         R"(<office:body><office:spreadsheet><table:table table:name="s">)"
         R"(<table:table-column/>)"
         R"(<table:table-column table:default-cell-style-name="Blue"/>)"
         R"(<table:table-column/>)"
         R"(<table:table-row>)" +
         string_cell("a", "ce1") + string_cell("b") +
         R"(</table:table-row>)"
         R"(<table:table-row table:number-rows-repeated="3">)"
         R"(<table:table-cell table:number-columns-repeated="3"/>)"
         R"(</table:table-row>)"
         R"(</table:table></office:spreadsheet></office:body>)"
         R"(</office:document>)";
}

} // namespace

TEST(OdfSheetStyle, a_row_style_reaches_every_cell_of_the_row) {
  const Document document = document_of(rows_and_columns());
  const Sheet sheet = first_sheet(document);

  sheet.set_row_style(0, fill(0x00ff00_rgb), bold());

  EXPECT_EQ(fill_at(sheet, 0, 0), 0x00ff00u);
  EXPECT_EQ(fill_at(sheet, 1, 0), 0x00ff00u);
  EXPECT_EQ(fill_at(sheet, 2, 0), 0x00ff00u);
  EXPECT_EQ(text_style_of(sheet, 0).font_weight, FontWeight::bold);
  EXPECT_EQ(fill_at(sheet, 1, 1), 0x0000ffu);
  EXPECT_EQ(fill_at(sheet, 0, 1), std::nullopt);
  // no row states a default, which LibreOffice would read as the sheet's
  EXPECT_EQ(saved(document).find("table:table-row table:default-cell-style"),
            std::string::npos);
}

TEST(OdfSheetStyle, a_row_style_keeps_what_each_cell_showed) {
  const Document document = document_of(rows_and_columns());
  const Sheet sheet = first_sheet(document);

  sheet.set_row_style(0, {}, bold());

  EXPECT_EQ(fill_at(sheet, 0, 0), 0xff0000u);
  EXPECT_EQ(fill_at(sheet, 1, 0), 0x0000ffu);
  EXPECT_EQ(fill_at(sheet, 2, 0), std::nullopt);
  EXPECT_EQ(text_style_of(sheet, 1).font_weight, FontWeight::bold);
}

/// Some producers state a row's default, which a cell without a style of its
/// own shows before its column's.
TEST(OdfSheetStyle, a_row_style_keeps_the_default_its_row_states) {
  std::string source = rows_and_columns();
  const std::string row = "<table:table-row>";
  source.replace(source.find(row), row.size(),
                 R"(<table:table-row table:default-cell-style-name="ce1">)");
  const Document document = document_of(source);
  const Sheet sheet = first_sheet(document);
  EXPECT_EQ(fill_at(sheet, 2, 0), 0xff0000u);

  sheet.set_row_style(0, {}, bold());

  EXPECT_EQ(fill_at(sheet, 1, 0), 0xff0000u);
  EXPECT_EQ(fill_at(sheet, 2, 0), 0xff0000u);
  EXPECT_EQ(text_style_of(sheet, 1).font_weight, FontWeight::bold);
}

TEST(OdfSheetStyle, a_row_style_cuts_a_repeated_row_and_reaches_past_it) {
  const Document document = document_of(rows_and_columns());
  const Sheet sheet = first_sheet(document);

  sheet.set_row_style(2, fill(0x00ff00_rgb), {});
  sheet.set_row_style(6, fill(0x00ff00_rgb), {});

  EXPECT_EQ(fill_at(sheet, 0, 1), std::nullopt);
  EXPECT_EQ(fill_at(sheet, 0, 2), 0x00ff00u);
  EXPECT_EQ(fill_at(sheet, 2, 2), 0x00ff00u);
  EXPECT_EQ(fill_at(sheet, 0, 3), std::nullopt);
  EXPECT_EQ(fill_at(sheet, 0, 5), std::nullopt);
  EXPECT_EQ(fill_at(sheet, 2, 6), 0x00ff00u);
}

TEST(OdfSheetStyle, a_column_style_reaches_every_cell_of_the_column) {
  const Document document = document_of(rows_and_columns());
  const Sheet sheet = first_sheet(document);

  sheet.set_column_style(0, fill(0x00ff00_rgb), {});
  sheet.set_column_style(1, {}, bold());

  EXPECT_EQ(fill_at(sheet, 0, 0), 0x00ff00u);
  EXPECT_EQ(fill_at(sheet, 0, 3), 0x00ff00u);
  EXPECT_EQ(fill_at(sheet, 0, 100), 0x00ff00u);
  EXPECT_EQ(fill_at(sheet, 1, 100), 0x0000ffu);
  EXPECT_EQ(fill_at(sheet, 2, 0), std::nullopt);
  EXPECT_EQ(text_style_of(sheet, 1).font_weight, FontWeight::bold);
  // the repeated row stays one: its cell stands for every row of it
  EXPECT_EQ(count(saved(document), "<table:table-row"), 2);
}

TEST(OdfSheetStyle, a_column_past_the_declared_ones_is_declared) {
  const Document document = document_of(rows_and_columns());
  const Sheet sheet = first_sheet(document);

  sheet.set_column_style(4, fill(0x00ff00_rgb), {});

  EXPECT_EQ(fill_at(sheet, 4, 50), 0x00ff00u);
  EXPECT_EQ(fill_at(sheet, 3, 50), std::nullopt);
}

TEST(OdfSheetStyle, the_ops_name_a_row_and_a_column) {
  const Document document = document_of(rows_and_columns());

  document.edit(R"({"version": 2, "ops": [)"
                R"({"op": "setRowStyle", "sheet": 0, "row": 1,)"
                R"( "style": {"fill": "#00ff00"}},)"
                R"({"op": "setColumnStyle", "sheet": 0, "column": 2,)"
                R"( "style": {"bold": true}}]})");

  const Sheet sheet = first_sheet(document);
  EXPECT_EQ(fill_at(sheet, 0, 1), 0x00ff00u);
  EXPECT_EQ(fill_at(sheet, 0, 2), std::nullopt);
  EXPECT_THROW(sheet.set_row_style(0,
                                   [] {
                                     TableCellStyle style;
                                     style.wrap_text = true;
                                     return style;
                                   }(),
                                   {}),
               UnsupportedOperation);
}

TEST(OdfSheetStyle, the_default_style_states_the_locale) {
  const auto locale_of = [](const std::string &properties) {
    return document_of(
               flat_sheet(string_cell("a"), "",
                          R"(<style:default-style style:family="table-cell">)"
                          R"(<style:text-properties )" +
                              properties + "/></style:default-style>"))
        .locale();
  };

  EXPECT_EQ(locale_of(R"(fo:language="de" fo:country="DE")"), "de-DE");
  EXPECT_EQ(locale_of(R"(fo:language="sr" fo:script="Latn" fo:country="RS")"),
            "sr-Latn-RS");
  EXPECT_EQ(locale_of(R"(fo:language="fr" fo:country="none")"), "fr");
  EXPECT_EQ(locale_of(R"(fo:language="zxx" fo:country="none")"), std::nullopt);
  EXPECT_EQ(locale_of(""), std::nullopt);
}
