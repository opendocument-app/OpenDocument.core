#include <odr/internal/odf/odf_chart.hpp>

#include <pugixml.hpp>

#include <gtest/gtest.h>

#include <optional>
#include <string>

using namespace odr::internal::odf;

namespace {

/// An embedded chart part, with @p chart wrapped in the document its
/// `content.xml` is.
std::string chart_part(const std::string &chart) {
  return R"(<office:document-content><office:automatic-styles>)"
         R"(<style:style style:name="s1" style:family="chart">)"
         R"(<style:graphic-properties draw:fill-color="#112233"/>)"
         R"(</style:style></office:automatic-styles>)"
         R"(<office:body><office:chart>)" +
         chart + R"(</office:chart></office:body></office:document-content>)";
}

/// Two series of two points, with one gap, laid out the way libreoffice writes
/// a `local-table`.
std::string bar_chart(const std::string &klass = "chart:bar",
                      const std::string &head = "",
                      const std::string &second_category = "two") {
  return chart_part(
      R"(<chart:chart svg:width="16cm" svg:height="9cm" chart:class=")" +
      klass + R"(">)" + head +
      R"(<chart:plot-area svg:x="1cm" svg:y="1cm" svg:width="14cm" svg:height="7cm">)"
      R"(<chart:series chart:style-name="s1"/><chart:series/>)"
      R"(</chart:plot-area>)"
      R"(<table:table table:name="local-table">)"
      R"(<table:table-header-columns><table:table-column/></table:table-header-columns>)"
      R"(<table:table-header-rows><table:table-row>)"
      R"(<table:table-cell><text:p/></table:table-cell>)"
      R"(<table:table-cell><text:p>alpha</text:p></table:table-cell>)"
      R"(<table:table-cell><text:p>beta</text:p></table:table-cell>)"
      R"(</table:table-row></table:table-header-rows>)"
      R"(<table:table-rows>)"
      R"(<table:table-row>)"
      R"(<table:table-cell><text:p>one</text:p></table:table-cell>)"
      R"(<table:table-cell office:value="1"><text:p>1</text:p></table:table-cell>)"
      R"(<table:table-cell office:value="4"><text:p>4</text:p></table:table-cell>)"
      R"(</table:table-row>)"
      R"(<table:table-row>)"
      R"(<table:table-cell><text:p>)" +
      second_category +
      R"(</text:p></table:table-cell>)"
      R"(<table:table-cell office:value="NaN"><text:p>NaN</text:p></table:table-cell>)"
      R"(<table:table-cell office:value="2"><text:p>2</text:p></table:table-cell>)"
      R"(</table:table-row>)"
      R"(</table:table-rows></table:table>)"
      R"(</chart:chart>)");
}

std::optional<std::string> render(const std::string &xml) {
  pugi::xml_document document;
  EXPECT_TRUE(document.load_string(xml.c_str()));
  return render_chart(document.document_element());
}

std::size_t count(const std::string &haystack, const std::string &needle) {
  std::size_t result = 0;
  for (std::size_t at = haystack.find(needle); at != std::string::npos;
       at = haystack.find(needle, at + 1)) {
    ++result;
  }
  return result;
}

} // namespace

TEST(OdfChart, a_chart_is_sized_in_hundredths_of_a_millimetre) {
  const std::optional<std::string> svg = render(bar_chart());
  ASSERT_TRUE(svg.has_value());
  EXPECT_NE(std::string::npos, svg->find(R"(viewBox="0 0 16000 9000")"));
  EXPECT_NE(std::string::npos, svg->find(R"(width="16cm")"));
}

TEST(OdfChart, the_first_column_names_the_categories) {
  const std::optional<std::string> svg = render(bar_chart());
  ASSERT_TRUE(svg.has_value());
  EXPECT_NE(std::string::npos, svg->find(">one<"));
  EXPECT_NE(std::string::npos, svg->find(">two<"));
}

TEST(OdfChart, the_header_row_names_the_series_in_the_legend) {
  const std::optional<std::string> svg = render(
      bar_chart("chart:bar", R"(<chart:legend svg:x="15cm" svg:y="1cm"/>)"));
  ASSERT_TRUE(svg.has_value());
  EXPECT_NE(std::string::npos, svg->find(">alpha<"));
  EXPECT_NE(std::string::npos, svg->find(">beta<"));
}

TEST(OdfChart, a_series_takes_the_colour_its_style_gives_it) {
  const std::optional<std::string> svg = render(bar_chart());
  ASSERT_TRUE(svg.has_value());
  EXPECT_NE(std::string::npos, svg->find("#112233"));
  // The second series has no style, so it falls to the palette's second entry.
  EXPECT_NE(std::string::npos, svg->find("#ff420e"));
}

TEST(OdfChart, a_nan_value_is_a_gap_rather_than_a_bar) {
  const std::optional<std::string> svg = render(bar_chart());
  ASSERT_TRUE(svg.has_value());
  // Three bars for four cells, plus the two the background and plot area are.
  EXPECT_EQ(5, count(*svg, "<rect"));
}

TEST(OdfChart, a_bar_axis_starts_at_zero_however_high_the_values_are) {
  const std::optional<std::string> svg = render(bar_chart());
  ASSERT_TRUE(svg.has_value());
  EXPECT_NE(std::string::npos, svg->find(">0<"));
}

TEST(OdfChart, a_line_chart_draws_one_path_per_series) {
  const std::optional<std::string> svg = render(bar_chart("chart:line"));
  ASSERT_TRUE(svg.has_value());
  EXPECT_EQ(2, count(*svg, "<path"));
  EXPECT_EQ(0, count(*svg, "nan"));
}

TEST(OdfChart, a_pie_slices_one_series_by_its_categories) {
  const std::optional<std::string> svg = render(bar_chart("chart:circle"));
  ASSERT_TRUE(svg.has_value());
  // One slice per point the first series has, and the gap is not one.
  EXPECT_EQ(1, count(*svg, "<path"));
}

TEST(OdfChart, a_category_label_is_skipped_where_it_would_not_fit) {
  const std::string long_category(60, 'x');
  const std::optional<std::string> svg =
      render(bar_chart("chart:bar", "", long_category));
  ASSERT_TRUE(svg.has_value());
  EXPECT_NE(std::string::npos, svg->find(">one<"));
  EXPECT_EQ(std::string::npos, svg->find(">" + long_category + "<"));
}

TEST(OdfChart, a_title_is_drawn) {
  const std::optional<std::string> svg =
      render(bar_chart("chart:bar", R"(<chart:title><text:p>Counts)"
                                    "\n"
                                    R"(</text:p></chart:title>)"));
  ASSERT_TRUE(svg.has_value());
  EXPECT_NE(std::string::npos, svg->find(">Counts<"));
}

TEST(OdfChart, a_part_with_no_chart_renders_nothing) {
  EXPECT_FALSE(render(chart_part("")).has_value());
  EXPECT_FALSE(
      render(chart_part(R"(<chart:chart svg:width="16cm" )"
                        R"(svg:height="9cm" chart:class="chart:bar"/>)"))
          .has_value());
}
