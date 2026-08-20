#include <odr/html.hpp>

#include <odr/internal/html/common.hpp>
#include <odr/internal/html/html_writer.hpp>

#include <gtest/gtest.h>

#include <optional>
#include <sstream>

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

std::string emit_zoom(const HtmlConfig &config, const bool fits,
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

TEST(html_common, fits_width_follows_the_resolved_mode) {
  HtmlConfig config;

  EXPECT_TRUE(ihtml::fits_width(config, true));
  EXPECT_FALSE(ihtml::fits_width(config, false));

  config.viewport_mode = HtmlViewportMode::fit_width;
  EXPECT_TRUE(ihtml::fits_width(config, false));

  config.viewport_mode = HtmlViewportMode::actual_size;
  EXPECT_FALSE(ihtml::fits_width(config, true));

  config.viewport_mode = HtmlViewportMode::automatic;
  EXPECT_TRUE(ihtml::fits_width(config, true, HtmlViewportMode::fit_width));
  EXPECT_FALSE(ihtml::fits_width(config, true, HtmlViewportMode::none));

  // the caller took the question over
  config.viewport_content = "width=420";
  EXPECT_FALSE(ihtml::fits_width(config, true));
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
  EXPECT_EQ(emit_zoom(config, true, 800),
            styled(":root{--odr-fit:0.5}body{zoom:0.5}"));
}

TEST(html_common, the_fit_never_scales_up) {
  HtmlConfig config;
  config.viewport_width = 1200;

  EXPECT_EQ(emit_zoom(config, true, 800), styled(""));
}

TEST(html_common, the_fit_is_left_to_the_view_where_a_width_is_missing) {
  HtmlConfig config;

  // no viewport width configured — the view measures itself instead
  EXPECT_EQ(emit_zoom(config, true, 800), styled(":root{--odr-fit:auto}"));

  config.viewport_width = 400;
  EXPECT_EQ(emit_zoom(config, true, std::nullopt),
            styled(":root{--odr-fit:auto}"));

  // nothing to fit: the view opens at actual size
  EXPECT_EQ(emit_zoom(config, false, 800), styled(""));
}

TEST(html_common, a_pinned_zoom_replaces_the_fit_it_opens_at) {
  HtmlConfig config;
  config.initial_zoom = 2;

  // nothing is fitted, so the pinned zoom stands alone
  EXPECT_EQ(emit_zoom(config, false, 800),
            styled(":root{--odr-zoom:2}body{zoom:2}"));

  // the fit is still stated, so `resetZoom()` has one to go back to
  config.viewport_width = 400;
  EXPECT_EQ(emit_zoom(config, true, 800),
            styled(":root{--odr-fit:0.5;--odr-zoom:2}body{zoom:2}"));
  EXPECT_EQ(emit_zoom(config, true, std::nullopt),
            styled(":root{--odr-fit:auto;--odr-zoom:2}body{zoom:2}"));
}

TEST(html_common, a_zoom_pinned_to_actual_size_is_still_a_pin) {
  HtmlConfig config;
  config.initial_zoom = 1;

  // nothing to apply, but the view has to know the fit was turned down
  EXPECT_EQ(emit_zoom(config, true, std::nullopt),
            styled(":root{--odr-fit:auto;--odr-zoom:1}"));

  config.viewport_width = 400;
  EXPECT_EQ(emit_zoom(config, true, 800),
            styled(":root{--odr-fit:0.5;--odr-zoom:1}"));

  // and a pin that lands on the fit is a pin too: a resize leaves it alone
  config.initial_zoom = 0.5;
  EXPECT_EQ(emit_zoom(config, true, 800),
            styled(":root{--odr-fit:0.5;--odr-zoom:0.5}body{zoom:0.5}"));
}
