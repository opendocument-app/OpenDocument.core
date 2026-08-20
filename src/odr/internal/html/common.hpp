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

/// Whether the output is meant to fit its width to the viewport — the same
/// question @ref write_viewport_meta answers with a meta tag, which a browser
/// honours for the top-level document only.
[[nodiscard]] bool
fits_width(const HtmlConfig &config, bool fit_width_by_default,
           std::optional<HtmlViewportMode> mode_override = {});

/// @p measure in css pixels (96 per inch), or nothing where it carries no
/// absolute unit.
[[nodiscard]] std::optional<double>
css_pixels(const std::optional<Measure> &measure);

/// The side gutters the page column puts around its widest page, in css
/// pixels — part of the width to fit, and the same in both renderers.
constexpr double page_column_gutter_pixels = 32;

/// Scales the body so that @p content_pixels of content fits
/// `config.viewport_width`. Writes nothing, and returns false, unless the
/// output fits its width and both widths are known — the load-time script is
/// what covers the rest.
bool write_viewport_fit_style(HtmlWriter &out, const HtmlConfig &config,
                              bool fits, std::optional<double> content_pixels);

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
