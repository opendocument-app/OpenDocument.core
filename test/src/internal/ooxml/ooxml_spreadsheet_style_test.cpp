#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_style.hpp>

#include <odr/internal/html/common.hpp>
#include <odr/internal/html/document_style.hpp>

#include <odr/style.hpp>

#include <array>
#include <stdexcept>
#include <string>
#include <utility>

#include <gtest/gtest.h>

#include <pugixml.hpp>

using namespace odr;
using namespace odr::internal;
using namespace odr::internal::ooxml::spreadsheet;

namespace {

constexpr const char *theme =
    R"(<a:theme><a:themeElements><a:clrScheme>)"
    R"(<a:dk1><a:sysClr lastClr="000000"/></a:dk1>)"
    R"(<a:lt1><a:sysClr lastClr="FFFFFF"/></a:lt1>)"
    R"(<a:dk2><a:srgbClr val="44546A"/></a:dk2>)"
    R"(<a:lt2><a:srgbClr val="E7E6E6"/></a:lt2>)"
    R"(<a:accent1><a:srgbClr val="4472C4"/></a:accent1>)"
    R"(</a:clrScheme></a:themeElements></a:theme>)";

/// The first `xf` of `cellXfs`, over @p fonts, @p fills and the `xf`
/// attributes and children in @p xf.
ResolvedStyle resolve(const std::string &fonts, const std::string &fills,
                      const std::string &xf) {
  pugi::xml_document styles;
  pugi::xml_document theme_document;
  const std::string xml = "<styleSheet><fonts>" + fonts + "</fonts><fills>" +
                          fills + "</fills><cellXfs>" + xf +
                          "</cellXfs></styleSheet>";
  EXPECT_TRUE(styles.load_string(xml.c_str()));
  EXPECT_TRUE(theme_document.load_string(theme));
  return StyleRegistry(styles.document_element(),
                       theme_document.document_element())
      .cell_style(0);
}

std::string fill_of(const std::string &fill) {
  const ResolvedStyle style =
      resolve("", "<fill>" + fill + "</fill>", R"(<xf fillId="0"/>)");
  const std::optional<Color> color = style.table_cell_style.background_color;
  return color.has_value() ? internal::html::color(*color) : "none";
}

TextStyle font_of(const std::string &font) {
  return resolve("<font>" + font + "</font>", "",
                 R"(<xf fontId="0" applyFont="1"/>)")
      .text_style;
}

TableCellStyle borders_of(const std::string &borders, const bool apply = true) {
  pugi::xml_document styles;
  pugi::xml_document theme_document;
  const std::string xml =
      "<styleSheet><borders><border>" + borders +
      "</border></borders><cellXfs><xf borderId=\"0\" applyBorder=\"" +
      (apply ? "1" : "0") + "\"/></cellXfs></styleSheet>";
  EXPECT_TRUE(styles.load_string(xml.c_str()));
  EXPECT_TRUE(theme_document.load_string(theme));
  return StyleRegistry(styles.document_element(),
                       theme_document.document_element())
      .cell_style(0)
      .table_cell_style;
}

} // namespace

TEST(OoxmlSpreadsheetStyle, a_solid_fill_paints_its_foreground) {
  EXPECT_EQ(fill_of(R"(<patternFill patternType="solid">)"
                    R"(<fgColor rgb="FFDEE6EF"/><bgColor rgb="FFCCFFFF"/>)"
                    R"(</patternFill>)"),
            "#dee6ef");
}

TEST(OoxmlSpreadsheetStyle, a_fill_of_no_pattern_paints_nothing) {
  EXPECT_EQ(fill_of(R"(<patternFill patternType="none"/>)"), "none");
  EXPECT_EQ(fill_of(R"(<patternFill><bgColor rgb="FFCCFFFF"/></patternFill>)"),
            "none");
}

TEST(OoxmlSpreadsheetStyle, a_theme_colour_counts_light_before_dark) {
  EXPECT_EQ(fill_of(R"(<patternFill patternType="solid">)"
                    R"(<fgColor theme="0"/></patternFill>)"),
            "#ffffff");
  EXPECT_EQ(fill_of(R"(<patternFill patternType="solid">)"
                    R"(<fgColor theme="4"/></patternFill>)"),
            "#4472c4");
}

// Excel's "Blue, Accent 1, Darker 25%" and "Lighter 80%".
TEST(OoxmlSpreadsheetStyle, a_tint_moves_the_lightness) {
  EXPECT_EQ(fill_of(R"(<patternFill patternType="solid">)"
                    R"(<fgColor theme="4" tint="-0.249977111117893"/>)"
                    R"(</patternFill>)"),
            "#2f5597");
  EXPECT_EQ(fill_of(R"(<patternFill patternType="solid">)"
                    R"(<fgColor theme="4" tint="0.79998168889431442"/>)"
                    R"(</patternFill>)"),
            "#dae3f3");
}

TEST(OoxmlSpreadsheetStyle, a_font_states_its_toggles) {
  const TextStyle on = font_of(R"(<b/><i/><u/><strike/>)");
  EXPECT_EQ(on.font_weight, FontWeight::bold);
  EXPECT_EQ(on.font_style, FontStyle::italic);
  EXPECT_EQ(on.font_underline, true);
  EXPECT_EQ(on.font_line_through, true);

  const TextStyle off =
      font_of(R"(<b val="0"/><i val="0"/><u val="none"/><strike val="0"/>)");
  EXPECT_EQ(off.font_weight, FontWeight::normal);
  EXPECT_EQ(off.font_style, FontStyle::normal);
  EXPECT_EQ(off.font_underline, false);
  EXPECT_EQ(off.font_line_through, false);
}

TEST(OoxmlSpreadsheetStyle, alignment_reads_every_side) {
  const auto align = [](const std::string &side, const std::string &value) {
    return resolve("", "",
                   R"(<xf applyAlignment="1"><alignment )" + side + "=\"" +
                       value + R"("/></xf>)")
        .table_cell_style;
  };
  EXPECT_EQ(align("horizontal", "left").horizontal_align,
            HorizontalAlign::left);
  EXPECT_EQ(align("horizontal", "right").horizontal_align,
            HorizontalAlign::right);
  EXPECT_EQ(align("horizontal", "general").horizontal_align, std::nullopt);
  EXPECT_EQ(align("vertical", "top").vertical_align, VerticalAlign::top);
  EXPECT_EQ(align("vertical", "bottom").vertical_align, VerticalAlign::bottom);
}

TEST(OoxmlSpreadsheetStyle, border_weights_and_patterns_reach_the_html) {
  const std::array<std::pair<const char *, const char *>, 13> cases{{
      {"hair", "0.25pt solid"},
      {"thin", "0.75pt solid"},
      {"medium", "1.5pt solid"},
      {"thick", "2.25pt solid"},
      {"double", "2.25pt double"},
      {"dotted", "0.75pt dotted"},
      {"dashed", "0.75pt dashed"},
      {"dashDot", "0.75pt dashed"},
      {"dashDotDot", "0.75pt dashed"},
      {"mediumDashed", "1.5pt dashed"},
      {"mediumDashDot", "1.5pt dashed"},
      {"mediumDashDotDot", "1.5pt dashed"},
      {"slantDashDot", "1.5pt dashed"},
  }};
  for (const auto &[name, expected] : cases) {
    SCOPED_TRACE(name);
    const TableCellStyle style =
        borders_of(std::string(R"(<left style=")") + name +
                   R"("><color rgb="FF123456"/></left>)");
    const std::string declaration = std::string(expected) + " #123456";
    EXPECT_EQ(style.border.left, declaration);
    EXPECT_NE(internal::html::translate_table_cell_style(style, false)
                  .find("border-left:" + declaration),
              std::string::npos);
  }
}

TEST(OoxmlSpreadsheetStyle, border_sides_keep_their_own_colour_and_style) {
  const TableCellStyle style = borders_of(
      R"(<left style="double"><color theme="4"/></left>)"
      R"(<right style="thick"><color indexed="2"/></right>)"
      R"(<top style="dotted"/><bottom style="none"><color rgb="FF123456"/></bottom>)");
  EXPECT_EQ(style.border.left, "2.25pt double #4472c4");
  EXPECT_EQ(style.border.right, "2.25pt solid #ff0000");
  EXPECT_EQ(style.border.top, "0.75pt dotted");
  EXPECT_EQ(style.border.bottom, "none");
  EXPECT_NE(internal::html::translate_table_cell_style(style, false)
                .find("border-bottom:none"),
            std::string::npos);
}

TEST(OoxmlSpreadsheetStyle, an_absent_or_disabled_border_is_not_painted) {
  EXPECT_FALSE(borders_of(R"(<left><color rgb="FF123456"/></left>)")
                   .border.left.has_value());
  EXPECT_FALSE(
      borders_of(R"(<left style="thick"/>)", false).border.left.has_value());
  EXPECT_THROW(borders_of(R"(<left style="not-a-border"/>)"),
               std::runtime_error);
}
