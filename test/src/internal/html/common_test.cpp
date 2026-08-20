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

std::string emit_fit(const HtmlConfig &config, const bool fits,
                     const std::optional<double> content_pixels) {
  std::ostringstream out;
  ihtml::HtmlWriter writer(out, false, "");
  ihtml::write_viewport_fit_style(writer, config, fits, content_pixels);
  return out.str();
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

  EXPECT_EQ(emit_fit(config, true, 800), "<style>body{zoom:0.5}</style>");
}

TEST(html_common, the_fit_never_scales_up) {
  HtmlConfig config;
  config.viewport_width = 1200;

  EXPECT_EQ(emit_fit(config, true, 800), "");
}

TEST(html_common, the_fit_needs_both_widths_and_a_reason_to_fit) {
  HtmlConfig config;

  // no viewport width configured — the load-time script covers it instead
  EXPECT_EQ(emit_fit(config, true, 800), "");

  config.viewport_width = 400;
  EXPECT_EQ(emit_fit(config, true, std::nullopt), "");
  EXPECT_EQ(emit_fit(config, false, 800), "");
}
