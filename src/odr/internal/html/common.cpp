#include <odr/internal/html/common.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/crypto/crypto_util.hpp>
#include <odr/internal/html/html_writer.hpp>
#include <odr/internal/util/stream_util.hpp>
#include <odr/internal/util/string_util.hpp>

#include <odr/html.hpp>
#include <odr/quantity.hpp>
#include <odr/style.hpp>

#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>

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
    out.write_header_viewport(
        "width=device-width,initial-scale=1.0,user-scalable=yes");
    break;
  case HtmlViewportMode::none:
  default:
    break;
  }
}

bool html::fits_width(const HtmlConfig &config, const bool fit_width_by_default,
                      const std::optional<HtmlViewportMode> mode_override) {
  // A raw `viewport_content` is the caller taking the question over.
  if (config.viewport_content.has_value()) {
    return false;
  }

  const HtmlViewportMode mode = mode_override.value_or(config.viewport_mode);
  if (mode == HtmlViewportMode::automatic) {
    return fit_width_by_default;
  }
  return mode == HtmlViewportMode::fit_width;
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

bool html::write_viewport_fit_style(
    HtmlWriter &out, const HtmlConfig &config, const bool fits,
    const std::optional<double> content_pixels) {
  if (!fits || !config.viewport_width.has_value() ||
      !content_pixels.has_value()) {
    return false;
  }

  const double factor =
      static_cast<double>(config.viewport_width.value()) / *content_pixels;
  // only ever down: a page narrower than the viewport is shown at its size
  if (factor >= 1) {
    return true;
  }

  out.write_header_style_begin();
  // `zoom` scales the layout, so the page scrolls against the scaled size
  // instead of overflowing beside it; `Measure` renders no exponent form
  out.out() << "body{zoom:" << Measure(factor, DynamicUnit()).to_string()
            << "}";
  out.write_header_style_end();

  return true;
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
