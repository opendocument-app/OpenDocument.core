#include <odr/internal/html/common.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/crypto/crypto_util.hpp>
#include <odr/internal/html/html_writer.hpp>
#include <odr/internal/util/stream_util.hpp>
#include <odr/internal/util/string_util.hpp>

#include <odr/html.hpp>
#include <odr/quantity.hpp>
#include <odr/style.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace odr::internal {

void html::write_viewport_meta(
    HtmlWriter &out, const HtmlConfig &config, const bool fit_width_by_default,
    const std::optional<HtmlViewportMode> mode_override) {
  if (config.viewport_content.has_value()) {
    out.write_header_viewport(
        escape_attribute(config.viewport_content.value()));
    return;
  }

  HtmlViewportMode mode = mode_override.value_or(config.viewport_mode);
  if (mode == HtmlViewportMode::automatic) {
    mode = fit_width_by_default ? HtmlViewportMode::fit_width
                                : HtmlViewportMode::actual_size;
  }

  switch (mode) {
  case HtmlViewportMode::fit_width:
    out.write_header_viewport("width=device-width,user-scalable=yes");
    break;
  case HtmlViewportMode::actual_size:
  // A stated scale is what turns the browser's own fitting off.
  case HtmlViewportMode::fit_width_by_view:
    out.write_header_viewport(
        "width=device-width,initial-scale=1.0,user-scalable=yes");
    break;
  case HtmlViewportMode::none:
  default:
    break;
  }
}

html::WidthFit
html::width_fit(const HtmlConfig &config, const bool fit_width_by_default,
                const std::optional<HtmlViewportMode> mode_override) {
  // A raw `viewport_content` is the caller taking the question over.
  if (config.viewport_content.has_value()) {
    return WidthFit::none;
  }

  const HtmlViewportMode mode = mode_override.value_or(config.viewport_mode);
  switch (mode) {
  case HtmlViewportMode::automatic:
    return fit_width_by_default ? WidthFit::browser : WidthFit::none;
  case HtmlViewportMode::fit_width:
    return WidthFit::browser;
  case HtmlViewportMode::fit_width_by_view:
    return WidthFit::view;
  default:
    return WidthFit::none;
  }
}

std::optional<double> html::css_pixels(const std::optional<Measure> &measure) {
  if (!measure.has_value()) {
    return {};
  }

  // css absolute lengths, all defined against the inch (css values 3, 5.2).
  static const std::unordered_map<std::string, double> per_unit{
      {"px", 1.0},      {"in", 96.0},        {"pt", 96.0 / 72.0},
      {"pc", 96.0 / 6}, {"cm", 96.0 / 2.54}, {"mm", 96.0 / 25.4},
  };

  const auto it = per_unit.find(std::string(measure->unit().name()));
  if (it == std::end(per_unit)) {
    return {};
  }
  const double pixels = measure->magnitude() * it->second;
  return pixels > 0 ? std::optional(pixels) : std::nullopt;
}

namespace {

/// Whether @p name is a unit css could read: letters, or a percent sign. Also
/// what keeps a hosted value from closing the `<style>` element it is written
/// into.
bool is_css_unit(const std::string_view name) {
  if (name.empty()) {
    return false;
  }
  if (name == "%") {
    return true;
  }
  return std::ranges::all_of(name, [](const char c) {
    return std::isalpha(static_cast<unsigned char>(c)) != 0;
  });
}

/// The margin a side states, where css can read it.
std::optional<Measure> css_margin(const std::optional<Measure> &measure) {
  if (!measure.has_value() || !is_css_unit(measure->unit().name())) {
    return {};
  }
  return measure;
}

} // namespace

double html::page_column_gutter_pixels(const HtmlConfig &config) {
  const auto side = [](const std::optional<Measure> &measure) {
    return std::max(page_column_side_gutter_pixels,
                    css_pixels(css_margin(measure)).value_or(0.0));
  };
  return side(config.min_content_margin.left) +
         side(config.min_content_margin.right);
}

void html::write_content_margin_style(HtmlWriter &out,
                                      const HtmlConfig &config) {
  const DirectionalStyle<Measure> &margin = config.min_content_margin;

  const std::array sides{
      std::pair{std::string_view("top"), css_margin(margin.top)},
      std::pair{std::string_view("right"), css_margin(margin.right)},
      std::pair{std::string_view("bottom"), css_margin(margin.bottom)},
      std::pair{std::string_view("left"), css_margin(margin.left)},
  };

  if (std::ranges::none_of(
          sides, [](const auto &side) { return side.second.has_value(); })) {
    return;
  }

  out.write_header_style_begin();
  out.out() << ":root{";
  for (const auto &[name, measure] : sides) {
    if (measure.has_value()) {
      out.out() << "--odr-min-margin-" << name << ":" << measure->to_string()
                << ";";
    }
  }
  out.out() << "}";
  out.write_header_style_end();
}

void html::write_zoom_style(HtmlWriter &out, const HtmlConfig &config,
                            const WidthFit fits,
                            const std::optional<double> content_pixels) {
  // The factor, or who measures it where the css cannot state it.
  std::optional<double> fit = 1;
  std::string_view measures;
  switch (fits) {
  case WidthFit::none:
    break;
  case WidthFit::browser:
    if (config.viewport_width.has_value() && content_pixels.has_value()) {
      // only ever down: a page narrower than the viewport is shown at its size
      fit = std::min(1.0, static_cast<double>(config.viewport_width.value()) /
                              *content_pixels);
    } else {
      fit.reset();
      measures = "auto";
    }
    break;
  case WidthFit::view:
    fit.reset();
    measures = "view";
    break;
  }

  const std::optional<double> zoom =
      config.initial_zoom.has_value() ? config.initial_zoom : fit;

  const auto number = [](const double value) {
    // `Measure` renders no exponent form
    return Measure(value, DynamicUnit()).to_string();
  };

  const bool writes_fit = !fit.has_value() || *fit != 1;
  // stated because it was set: `1` is actual size asked for, not the fit
  const bool writes_pin = config.initial_zoom.has_value();
  const bool writes_body_zoom = zoom.has_value() && *zoom != 1;

  out.write_header_style_begin();

  if (writes_fit || writes_pin) {
    out.out() << ":root{";
    if (writes_fit) {
      out.out() << "--odr-fit:";
      if (fit.has_value()) {
        out.out() << number(*fit);
      } else {
        out.out() << measures;
      }
      if (writes_pin) {
        out.out() << ";";
      }
    }
    if (writes_pin) {
      out.out() << "--odr-zoom:" << number(*config.initial_zoom);
    }
    out.out() << "}";
  }

  if (writes_body_zoom) {
    // `zoom` scales the layout, so the page scrolls against the scaled size
    // instead of overflowing beside it
    out.out() << "body{zoom:" << number(*zoom) << "}";
  }

  // paper has its own geometry; beats the script's inline zoom
  out.out() << "@media print{:root{--odr-zoom:1!important}"
               "body{zoom:1!important}}";

  out.write_header_style_end();
}

std::string html::escape_text(std::string text) {
  if (text.empty()) {
    return text;
  }

  util::string::replace_all(text, "&", "&amp;");
  util::string::replace_all(text, "<", "&lt;");
  util::string::replace_all(text, ">", "&gt;");

  if (text.front() == ' ') {
    text = "&nbsp;" + text.substr(1);
  }
  if (text.back() == ' ') {
    text = text.substr(0, text.length() - 1) + "&nbsp;";
  }
  util::string::replace_all(text, "  ", " &nbsp;");

  // TODO `&emsp;` is not a tab
  util::string::replace_all(text, "\t", "&emsp;");

  return text;
}

std::string html::escape_attribute(std::string value) {
  util::string::replace_all(value, "&", "&amp;");
  util::string::replace_all(value, "\"", "&quot;");
  util::string::replace_all(value, "<", "&lt;");
  util::string::replace_all(value, ">", "&gt;");
  return value;
}

std::string
html::fill_path_variables(const std::string &path,
                          const std::optional<std::uint32_t> index) {
  std::string result = path;
  util::string::replace_all(result, "{index}",
                            index ? std::to_string(*index) : "");
  return result;
}

std::string html::color(const Color &color) {
  if (color.alpha != 255) {
    std::stringstream ss;
    ss << "rgba(" << static_cast<std::uint32_t>(color.red) << ","
       << static_cast<std::uint32_t>(color.green) << ","
       << static_cast<std::uint32_t>(color.blue) << ","
       << (static_cast<double>(color.alpha) / 255.0) << ")";
    return ss.str();
  }
  std::stringstream ss;
  ss << "#";
  ss << std::setw(6) << std::setfill('0') << std::hex << color.rgb();
  return ss.str();
}

std::string html::file_to_url(const std::string &file,
                              const std::string &mime_type) {
  return "data:" + mime_type + ";base64," + crypto::util::base64_encode(file);
}

std::string html::file_to_url(std::istream &file,
                              const std::string &mime_type) {
  return file_to_url(util::stream::read(file), mime_type);
}

std::string html::file_to_url(const abstract::File &file,
                              const std::string &mime_type) {
  return file_to_url(*file.stream(), mime_type);
}

} // namespace odr::internal
