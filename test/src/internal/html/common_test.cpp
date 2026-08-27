#include <odr/html.hpp>
#include <odr/style.hpp>

#include <odr/internal/html/common.hpp>
#include <odr/internal/html/html_writer.hpp>

#include <gtest/gtest.h>

#include <limits>
#include <optional>
#include <sstream>
#include <string>

using namespace odr;
namespace ihtml = odr::internal::html;

namespace {

std::string
emit_viewport(const HtmlConfig &config, const bool fit_width_by_default,
              const std::optional<HtmlViewportMode> mode_override = {}) {
  std::ostringstream out;
  ihtml::HtmlWriter writer(out, false, "");
  ihtml::write_viewport_meta(writer, config, fit_width_by_default,
                             mode_override);
  return out.str();
}

constexpr const char *fit_width =
    R"(<meta name="viewport" content="width=device-width,user-scalable=yes"/>)";
constexpr const char *actual_size =
    R"(<meta name="viewport" content="width=device-width,initial-scale=1.0,user-scalable=yes"/>)";

} // namespace

TEST(html_common, viewport_automatic_resolves_per_content) {
  const HtmlConfig config;
  EXPECT_EQ(emit_viewport(config, true), fit_width);
  EXPECT_EQ(emit_viewport(config, false), actual_size);
}

TEST(html_common, viewport_mode_beats_automatic_default) {
  HtmlConfig config;
  config.viewport_mode = HtmlViewportMode::actual_size;
  EXPECT_EQ(emit_viewport(config, true), actual_size);
  config.viewport_mode = HtmlViewportMode::fit_width;
  EXPECT_EQ(emit_viewport(config, false), fit_width);
}

TEST(html_common, fit_width_by_view_pins_the_scale_the_browser_would_fit) {
  HtmlConfig config;
  config.viewport_mode = HtmlViewportMode::fit_width_by_view;
  // the view fits it, so the browser is told not to
  EXPECT_EQ(emit_viewport(config, true), actual_size);
  EXPECT_EQ(emit_viewport(config, false), actual_size);
}

TEST(html_common, viewport_mode_none_writes_nothing) {
  HtmlConfig config;
  config.viewport_mode = HtmlViewportMode::none;
  EXPECT_EQ(emit_viewport(config, true), "");
}

TEST(html_common, viewport_mode_override_beats_mode) {
  HtmlConfig config;
  config.viewport_mode = HtmlViewportMode::fit_width;
  EXPECT_EQ(emit_viewport(config, true, HtmlViewportMode::actual_size),
            actual_size);
  // `automatic` as override resolves per content again
  EXPECT_EQ(emit_viewport(config, false, HtmlViewportMode::automatic),
            actual_size);
}

TEST(html_common, viewport_content_beats_modes_and_is_escaped) {
  HtmlConfig config;
  config.viewport_mode = HtmlViewportMode::none;
  config.viewport_content = R"(width=650,"a"&<b>)";
  EXPECT_EQ(
      emit_viewport(config, true),
      R"(<meta name="viewport" content="width=650,&quot;a&quot;&amp;&lt;b&gt;"/>)");
}

namespace {

std::string emit_zoom(const HtmlConfig &config, const ihtml::WidthFit fits,
                      const std::optional<double> content_pixels) {
  std::ostringstream out;
  ihtml::HtmlWriter writer(out, false, "");
  ihtml::write_zoom_style(writer, config, fits, content_pixels);
  return out.str();
}

/// Written whatever the zoom is, so every case states it.
constexpr const char *print =
    "@media print{:root{--odr-zoom:1!important}body{zoom:1!important}}";

std::string styled(const std::string &css) {
  return "<style>" + css + print + "</style>";
}

} // namespace

TEST(html_common, width_fit_follows_the_resolved_mode) {
  using ihtml::WidthFit;
  HtmlConfig config;

  EXPECT_EQ(ihtml::width_fit(config, true), WidthFit::browser);
  EXPECT_EQ(ihtml::width_fit(config, false), WidthFit::none);

  config.viewport_mode = HtmlViewportMode::fit_width;
  EXPECT_EQ(ihtml::width_fit(config, false), WidthFit::browser);

  config.viewport_mode = HtmlViewportMode::actual_size;
  EXPECT_EQ(ihtml::width_fit(config, true), WidthFit::none);

  config.viewport_mode = HtmlViewportMode::fit_width_by_view;
  EXPECT_EQ(ihtml::width_fit(config, false), WidthFit::view);

  config.viewport_mode = HtmlViewportMode::automatic;
  EXPECT_EQ(ihtml::width_fit(config, true, HtmlViewportMode::fit_width),
            WidthFit::browser);
  EXPECT_EQ(ihtml::width_fit(config, true, HtmlViewportMode::none),
            WidthFit::none);
  EXPECT_EQ(
      ihtml::width_fit(config, false, HtmlViewportMode::fit_width_by_view),
      WidthFit::view);

  // the caller took the question over
  config.viewport_content = "width=420";
  EXPECT_EQ(ihtml::width_fit(config, true), WidthFit::none);
}

TEST(html_common, css_pixels_converts_the_absolute_units) {
  EXPECT_EQ(ihtml::css_pixels(Measure(1, DynamicUnit("in"))), 96.0);
  EXPECT_EQ(ihtml::css_pixels(Measure(72, DynamicUnit("pt"))), 96.0);
  EXPECT_EQ(ihtml::css_pixels(Measure(2.54, DynamicUnit("cm"))), 96.0);
  EXPECT_EQ(ihtml::css_pixels(Measure(25.4, DynamicUnit("mm"))), 96.0);
  EXPECT_EQ(ihtml::css_pixels(Measure(96, DynamicUnit("px"))), 96.0);

  EXPECT_FALSE(ihtml::css_pixels(std::nullopt).has_value());
  EXPECT_FALSE(ihtml::css_pixels(Measure(50, DynamicUnit("%"))).has_value());
  EXPECT_FALSE(ihtml::css_pixels(Measure(0, DynamicUnit("in"))).has_value());
}

TEST(html_common, the_fit_scales_the_body_to_the_configured_viewport) {
  HtmlConfig config;
  config.viewport_width = 400;

  // `--odr-zoom` states a pin, and nothing pinned this one
  EXPECT_EQ(emit_zoom(config, ihtml::WidthFit::browser, 800),
            styled(":root{--odr-fit:0.5}body{zoom:0.5}"));
}

TEST(html_common, the_fit_never_scales_up) {
  HtmlConfig config;
  config.viewport_width = 1200;

  EXPECT_EQ(emit_zoom(config, ihtml::WidthFit::browser, 800), styled(""));
}

TEST(html_common, the_fit_is_left_to_the_view_where_a_width_is_missing) {
  HtmlConfig config;

  // no viewport width configured — the view measures itself instead
  EXPECT_EQ(emit_zoom(config, ihtml::WidthFit::browser, 800),
            styled(":root{--odr-fit:auto}"));

  config.viewport_width = 400;
  EXPECT_EQ(emit_zoom(config, ihtml::WidthFit::browser, std::nullopt),
            styled(":root{--odr-fit:auto}"));

  // nothing to fit: the view opens at actual size
  EXPECT_EQ(emit_zoom(config, ihtml::WidthFit::none, 800), styled(""));
}

TEST(html_common, the_view_measures_the_fit_where_it_was_asked_to) {
  HtmlConfig config;

  EXPECT_EQ(emit_zoom(config, ihtml::WidthFit::view, 800),
            styled(":root{--odr-fit:view}"));

  // a stated width does not turn it back into a factor the css writes
  config.viewport_width = 400;
  EXPECT_EQ(emit_zoom(config, ihtml::WidthFit::view, 800),
            styled(":root{--odr-fit:view}"));
}

TEST(html_common, a_pinned_zoom_replaces_the_fit_it_opens_at) {
  HtmlConfig config;
  config.initial_zoom = 2;

  // nothing is fitted, so the pinned zoom stands alone
  EXPECT_EQ(emit_zoom(config, ihtml::WidthFit::none, 800),
            styled(":root{--odr-zoom:2}body{zoom:2}"));

  // the fit is still stated, so `resetZoom()` has one to go back to
  config.viewport_width = 400;
  EXPECT_EQ(emit_zoom(config, ihtml::WidthFit::browser, 800),
            styled(":root{--odr-fit:0.5;--odr-zoom:2}body{zoom:2}"));
  EXPECT_EQ(emit_zoom(config, ihtml::WidthFit::browser, std::nullopt),
            styled(":root{--odr-fit:auto;--odr-zoom:2}body{zoom:2}"));
}

TEST(html_common, a_zoom_pinned_to_actual_size_is_still_a_pin) {
  HtmlConfig config;
  config.initial_zoom = 1;

  // nothing to apply, but the view has to know the fit was turned down
  EXPECT_EQ(emit_zoom(config, ihtml::WidthFit::browser, std::nullopt),
            styled(":root{--odr-fit:auto;--odr-zoom:1}"));

  config.viewport_width = 400;
  EXPECT_EQ(emit_zoom(config, ihtml::WidthFit::browser, 800),
            styled(":root{--odr-fit:0.5;--odr-zoom:1}"));

  // and a pin that lands on the fit is a pin too: a resize leaves it alone
  config.initial_zoom = 0.5;
  EXPECT_EQ(emit_zoom(config, ihtml::WidthFit::browser, 800),
            styled(":root{--odr-fit:0.5;--odr-zoom:0.5}body{zoom:0.5}"));
}

namespace {

std::string emit_margin(const HtmlConfig &config) {
  std::ostringstream out;
  ihtml::HtmlWriter writer(out, false, "");
  ihtml::write_content_margin_style(writer, config);
  return out.str();
}

} // namespace

TEST(html_common, the_content_margin_states_the_sides_that_are_set) {
  HtmlConfig config;

  // unset it declares nothing, which leaves the shipped insets as they are
  EXPECT_EQ(emit_margin(config), "");

  config.min_content_margin.top = Measure("12px");
  config.min_content_margin.left = Measure("1cm");
  EXPECT_EQ(emit_margin(config), "<style>:root{--odr-min-margin-top:12px;"
                                 "--odr-min-margin-left:1cm;}</style>");
}

// The stylesheets floor their insets with `max(16px,var(--odr-min-margin-*))`,
// so a value css cannot resolve voids the whole shorthand rather than one side
// — it has to be dropped here instead.
TEST(html_common, the_content_margin_drops_what_css_cannot_read_as_a_length) {
  HtmlConfig config;

  const auto emit = [&](const Measure &measure) {
    config.min_content_margin = {};
    config.min_content_margin.top = measure;
    return emit_margin(config);
  };

  // a unit that is neither a css length nor closes the style element
  EXPECT_EQ(emit(Measure("1foo")), "");
  EXPECT_EQ(emit(Measure("1px;}</style><script>alert(1)</script>")), "");
  EXPECT_EQ(emit(Measure(1, DynamicUnit())), "");

  // a magnitude that renders no length
  EXPECT_EQ(emit(Measure(-1, DynamicUnit("px"))), "");
  EXPECT_EQ(emit(Measure(0, DynamicUnit("px"))), "");
  EXPECT_EQ(
      emit(Measure(std::numeric_limits<double>::infinity(), DynamicUnit("px"))),
      "");
  EXPECT_EQ(emit(Measure(std::numeric_limits<double>::quiet_NaN(),
                         DynamicUnit("px"))),
            "");

  // what css does read is written verbatim, unit case and all
  EXPECT_NE(emit(Measure("2EM")).find("--odr-min-margin-top:2EM;"),
            std::string::npos);
  EXPECT_NE(emit(Measure("50%")).find("--odr-min-margin-top:50%;"),
            std::string::npos);
}

TEST(html_common, the_page_column_gutter_is_raised_but_never_lowered) {
  HtmlConfig config;

  // the 16px per side the stylesheets state
  EXPECT_EQ(ihtml::page_column_gutter_pixels(config), 32);

  config.min_content_margin.left = Measure("100px");
  EXPECT_EQ(ihtml::page_column_gutter_pixels(config), 116);

  config.min_content_margin.right = Measure("100PX");
  EXPECT_EQ(ihtml::page_column_gutter_pixels(config), 200);

  // a smaller floor changes nothing, and the top and bottom are no gutter
  config.min_content_margin.left = Measure("1px");
  config.min_content_margin.right = Measure("1px");
  config.min_content_margin.top = Measure("100px");
  EXPECT_EQ(ihtml::page_column_gutter_pixels(config), 32);

  // css applies these, the fit cannot count them
  config.min_content_margin.left = Measure("100em");
  config.min_content_margin.right = Measure("50%");
  EXPECT_EQ(ihtml::page_column_gutter_pixels(config), 32);
}

TEST(html_common, an_opaque_color_is_a_hex_triplet) {
  EXPECT_EQ(ihtml::color(Color(1, 2, 3)), "#010203");
  EXPECT_EQ(ihtml::color(Color(1, 2, 3, 255)), "#010203");
  EXPECT_EQ(ihtml::color(Color(0xff, 0xff, 0xff)), "#ffffff");
}

TEST(html_common, a_color_that_does_not_fully_cover_states_its_alpha) {
  EXPECT_EQ(ihtml::color(Color(1, 2, 3, 0)), "rgba(1,2,3,0)");
  EXPECT_EQ(ihtml::color(Color(1, 2, 3, 128)), "rgba(1,2,3,0.501961)");
  EXPECT_EQ(ihtml::color(Color(1, 2, 3, 1)), "rgba(1,2,3,0.00392157)");
}
