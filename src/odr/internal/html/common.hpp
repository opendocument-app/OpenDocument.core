#pragma once

#include <cstdint>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>

#include <odr/html.hpp>
#include <odr/internal/abstract/html_service.hpp>
#include <odr/quantity.hpp>

namespace odr {
struct Color;
struct HtmlConfig;
class Html;
} // namespace odr

namespace odr::internal::abstract {
class File;
}

namespace odr::internal::html {

struct WritingState {
  WritingState(HtmlWriter &out, const HtmlConfig &config,
               HtmlResources &resources)
      : m_out{&out}, m_config{&config}, m_resources(&resources) {}

  [[nodiscard]] HtmlWriter &out() const { return *m_out; }
  [[nodiscard]] const HtmlConfig &config() const { return *m_config; }
  [[nodiscard]] HtmlResources &resources() const { return *m_resources; }

private:
  HtmlWriter *m_out;
  const HtmlConfig *m_config;
  HtmlResources *m_resources;
};

/// Writes the viewport meta tag. Precedence: `config.viewport_content` (raw,
/// attribute-escaped), then `mode_override` (e.g.
/// `config.spreadsheet_viewport_mode` for spreadsheet content), then
/// `config.viewport_mode`. `fit_width_by_default` resolves
/// `HtmlViewportMode::automatic`: true for fixed-size paged content, false for
/// content that reflows to the screen width.
void write_viewport_meta(HtmlWriter &out, const HtmlConfig &config,
                         bool fit_width_by_default,
                         std::optional<HtmlViewportMode> mode_override = {});

/// Who fits the output's width to the viewport.
enum class WidthFit {
  none,    ///< nobody: it is shown at its size
  browser, ///< the browser, steered by the viewport meta tag
  view,    ///< the view itself, measured and kept current
};

[[nodiscard]] WidthFit
width_fit(const HtmlConfig &config, bool fit_width_by_default,
          std::optional<HtmlViewportMode> mode_override = {});

/// @p measure in css pixels, or nothing without an absolute unit.
[[nodiscard]] std::optional<double>
css_pixels(const std::optional<Measure> &measure);

/// Both side gutters the page column puts around its pages, in css pixels,
/// raised where `config.min_content_margin` asks for more. A side the css
/// resolves but this cannot — `em`, `%` — keeps the built-in gutter here.
[[nodiscard]] double page_column_gutter_pixels(const HtmlConfig &config);

/// The zoom the view opens at: `--odr-fit` fits @p content_pixels into
/// `config.viewport_width`, or names who measures it instead, `--odr-zoom` pins
/// it, `body{zoom}` applies the winner.
void write_zoom_style(HtmlWriter &out, const HtmlConfig &config, WidthFit fits,
                      std::optional<double> content_pixels);

/// `config.min_content_margin` as the `--odr-min-margin-*` the stylesheets
/// floor their insets against; nothing at all where no side states a css
/// length, which leaves those insets as shipped.
void write_content_margin_style(HtmlWriter &out, const HtmlConfig &config);

std::string escape_text(std::string text);

/// Escape a string for use as an HTML double-quoted attribute value (`&`, `"`,
/// `<`, `>`). Unlike `escape_text`, it leaves leading/trailing spaces intact.
std::string escape_attribute(std::string value);

/// What a target is, as an `href` would be dispatched. Whitespace and control
/// bytes are skipped while reading the scheme, as browsers strip them first.
enum class UriKind {
  relative, ///< no scheme
  external, ///< a navigable scheme
  refused,  ///< `javascript:` and kin
};

[[nodiscard]] UriKind uri_kind(std::string_view uri);

/// Safe to emit as an `href`.
[[nodiscard]] inline bool is_safe_uri(const std::string_view uri) {
  return uri_kind(uri) != UriKind::refused;
}

/// The `<a>` attributes for @p kind, unprefixed; empty but for
/// @ref UriKind::external.
[[nodiscard]] std::string_view link_target_attributes(UriKind kind);

std::string color(const Color &color);

/// Substitute `{index}` in an output file name pattern (e.g.
/// `page{index}.html`) with `index`, or with "" when absent.
std::string fill_path_variables(const std::string &path,
                                std::optional<std::uint32_t> index = {});

std::string file_to_url(const std::string &file, const std::string &mime_type);
std::string file_to_url(std::istream &file, const std::string &mime_type);
std::string file_to_url(const abstract::File &file,
                        const std::string &mime_type);

} // namespace odr::internal::html
