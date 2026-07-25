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
