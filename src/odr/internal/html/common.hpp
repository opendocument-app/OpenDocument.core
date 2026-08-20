#pragma once

#include <cstdint>
#include <iosfwd>
#include <optional>
#include <string>

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

/// Whether the output is meant to fit its width to the viewport.
[[nodiscard]] bool
fits_width(const HtmlConfig &config, bool fit_width_by_default,
           std::optional<HtmlViewportMode> mode_override = {});

/// @p measure in css pixels, or nothing without an absolute unit.
[[nodiscard]] std::optional<double>
css_pixels(const std::optional<Measure> &measure);

/// The side gutters the page column puts around its pages, in css pixels.
constexpr double page_column_gutter_pixels = 32;

/// The zoom the view opens at: `--odr-fit` fits @p content_pixels into
/// `config.viewport_width` (`auto` where only the view can measure it),
/// `--odr-zoom` pins it, `body{zoom}` applies the winner.
void write_zoom_style(HtmlWriter &out, const HtmlConfig &config, bool fits,
                      std::optional<double> content_pixels);

std::string escape_text(std::string text);

/// Escape a string for use as an HTML double-quoted attribute value (`&`, `"`,
/// `<`, `>`). Unlike `escape_text`, it leaves leading/trailing spaces intact.
std::string escape_attribute(std::string value);

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
