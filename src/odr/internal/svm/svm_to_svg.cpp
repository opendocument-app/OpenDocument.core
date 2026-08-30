#include <odr/internal/svm/svm_to_svg.hpp>

#include <odr/exceptions.hpp>
#include <odr/logger.hpp>

#include <odr/internal/crypto/crypto_util.hpp>
#include <odr/internal/svg/svg_writer.hpp>
#include <odr/internal/svm/svm_file.hpp>
#include <odr/internal/svm/svm_format.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace odr::internal::svm {

namespace {

/// Beyond this the nesting is a broken file, not a drawing.
constexpr std::size_t max_push_depth = 1024;

std::string action_name(const ActionHeader &action_header) {
  return std::string(action_type_name(action_header.type)) + "(" +
         std::to_string(action_header.type) + ")";
}

/// What an action reads and a `PUSH` saves, grouped by `PushFlags` bit.
struct GraphicsState final {
  MapMode map_mode;
  Font font;
  TextEncoding encoding{RTL_TEXTENCODING_ASCII_US};
  std::uint32_t line_rgb{};
  bool line_rgb_set{};
  std::uint32_t fill_rgb{};
  bool fill_rgb_set{};
  std::uint32_t text_rgb{};
  std::uint32_t text_fill_rgb{};
  bool text_fill_rgb_set{};
  std::uint32_t over_line_rgb{};
  /// vcl's default, and what a file that never says otherwise draws with.
  std::uint16_t text_align{ALIGN_TOP};
  /// What the drawing is clipped to, as path data, one entry per region the
  /// file intersected in. Nesting a group per entry is what intersects them.
  std::vector<std::string> clip;
};

struct SavedState final {
  GraphicsState state;
  /// What the `PUSH` named for restoring.
  std::uint16_t flags{};
};

struct Context final {
  svg::SvgWriter *out{};
  const Logger *logger{};

  GraphicsState state;
  std::vector<SavedState> stack;

  /// Names the masks and clip paths apart.
  std::uint32_t element_count{};
  bool inverter_written{};

  /// The clip the open groups already apply - a prefix of the state's clip
  /// once @ref ensure_clip has run.
  std::vector<std::string> written_clip;
};

double scale(const IntPair fraction) {
  return static_cast<double>(fraction.x) / fraction.y;
}

double transform_x(const std::int32_t x, const Context &context) {
  const MapMode &map_mode = context.state.map_mode;
  return (map_mode.origin.x + x) * scale(map_mode.scale_x);
}

double transform_y(const std::int32_t y, const Context &context) {
  const MapMode &map_mode = context.state.map_mode;
  return (map_mode.origin.y + y) * scale(map_mode.scale_y);
}

/// A length carries no origin, only the scale. The x scale even for a stroke
/// width or a dash, which lie along no axis: `svgwriter.cxx` maps those
/// through `ImplMap(sal_Int32)`, which takes the `Width()` of a square.
double transform_width(const std::int32_t width, const Context &context) {
  return width * scale(context.state.map_mode.scale_x);
}

double transform_height(const std::int32_t height, const Context &context) {
  return height * scale(context.state.map_mode.scale_y);
}

std::string get_svg_color_string(const std::uint32_t color) {
  const std::uint8_t blue = color >> 0 & 0xff;
  const std::uint8_t green = color >> 8 & 0xff;
  const std::uint8_t red = color >> 16 & 0xff;
  return "rgb(" + std::to_string(red) + "," + std::to_string(green) + "," +
         std::to_string(blue) + ")";
}

void write_color_style(svg::SvgWriter &out, const std::string_view property,
                       const std::uint32_t color, const bool set) {
  out.write_style(property, set ? get_svg_color_string(color) : "none");
}

/// Nothing set: a hairline, drawn solid.
bool is_default(const LineInfo &line_info) {
  return line_info.width == 0 && line_info.line_style != LINE_DASH;
}

/// `LineInfo::GetDotDashArray`: every dash, then every dot, each followed by
/// the distance to the next.
std::string get_dash_array_string(const LineInfo &line_info,
                                  const Context &context) {
  std::string result;
  const auto append = [&](const std::int32_t length) {
    if (!result.empty()) {
      result += ",";
    }
    result += svg::format_number(transform_width(length, context));
    result += ",";
    result += svg::format_number(transform_width(line_info.distance, context));
  };

  for (std::uint16_t i = 0; i < line_info.dash_count; ++i) {
    append(line_info.dash_length);
  }
  for (std::uint16_t i = 0; i < line_info.dot_count; ++i) {
    append(line_info.dot_length);
  }

  return result;
}

/// `basegfx::B2DLineJoin`.
std::string_view get_line_join_string(const std::uint16_t line_join) {
  switch (line_join) {
  case 2:
    return "bevel";
  case 4:
    return "round";
  default:
    return "miter";
  }
}

/// The state's line colour, plus the width, join and dashing of the action's
/// own `LineInfo` where it carries one.
void write_stroke_style(svg::SvgWriter &out, const Context &context,
                        const LineInfo *line_info) {
  const GraphicsState &state = context.state;
  write_color_style(out, "stroke", state.line_rgb, state.line_rgb_set);

  if (line_info == nullptr || is_default(*line_info)) {
    // one device pixel wide however far the drawing is scaled
    out.write_style("vector-effect", "non-scaling-stroke");
    return;
  }

  out.write_style("stroke-width", transform_width(line_info->width, context));
  out.write_style("stroke-linejoin",
                  get_line_join_string(line_info->line_join));
  if (line_info->line_style == LINE_DASH) {
    if (const std::string dash_array =
            get_dash_array_string(*line_info, context);
        !dash_array.empty()) {
      out.write_style("stroke-dasharray", dash_array);
    }
  }
}

/// `evenodd` is what cuts a shape's sub-polygons out as holes.
void write_fill_style(svg::SvgWriter &out, const Context &context) {
  write_color_style(out, "fill", context.state.fill_rgb,
                    context.state.fill_rgb_set);
  out.write_style("fill-rule", "evenodd");
}

/// `SVGAttributeWriter::SetFontAttr`'s mapping of `FontWeight`.
std::uint16_t get_font_weight(const std::uint16_t weight) {
  switch (weight) {
  case WEIGHT_THIN:
    return 100;
  case WEIGHT_ULTRALIGHT:
    return 200;
  case WEIGHT_LIGHT:
    return 300;
  case WEIGHT_MEDIUM:
    return 500;
  case WEIGHT_SEMIBOLD:
    return 600;
  case WEIGHT_BOLD:
    return 700;
  case WEIGHT_ULTRABOLD:
    return 800;
  case WEIGHT_BLACK:
    return 900;
  default:
    return 400;
  }
}

/// Which edge of the text the draw point names. Named rather than computed:
/// `svgwriter.cxx` shifts the point by the ascent, having the font metrics
/// that we do not.
std::string_view get_dominant_baseline(const std::uint16_t text_align) {
  switch (text_align) {
  case ALIGN_TOP:
    return "text-before-edge";
  case ALIGN_BOTTOM:
    return "text-after-edge";
  default:
    return "alphabetic";
  }
}

void write_text_style(svg::SvgWriter &out, const Context &context) {
  const GraphicsState &state = context.state;
  const Font &font = state.font;

  write_color_style(out, "fill", state.text_rgb, true);
  out.write_style("stroke", "none");
  out.write_style("font-family", font.family_name);
  out.write_style("font-size",
                  std::abs(transform_height(font.size.y, context)));
  out.write_style("dominant-baseline", get_dominant_baseline(state.text_align));

  if (font.italic == ITALIC_OBLIQUE) {
    out.write_style("font-style", "oblique");
  } else if (font.italic == ITALIC_NORMAL) {
    out.write_style("font-style", "italic");
  }
  if (const std::uint16_t weight = get_font_weight(font.weight);
      weight != 400) {
    out.write_style("font-weight", std::to_string(weight));
  }
  if (font.underline != LINESTYLE_NONE && font.strikeout != STRIKEOUT_NONE) {
    out.write_style("text-decoration", "underline line-through");
  } else if (font.underline != LINESTYLE_NONE) {
    out.write_style("text-decoration", "underline");
  } else if (font.strikeout != STRIKEOUT_NONE) {
    out.write_style("text-decoration", "line-through");
  }
}

void write_shape_style(svg::SvgWriter &out, const Context &context,
                       const bool fill, const LineInfo *line_info = nullptr) {
  write_stroke_style(out, context, line_info);
  if (fill) {
    write_fill_style(out, context);
  } else {
    out.write_style("fill", "none");
  }
}

void write_rectangle(const Rectangle &rect, const Context &context,
                     const std::uint32_t horizontal_round = 0,
                     const std::uint32_t vertical_round = 0) {
  svg::SvgWriter &out = *context.out;

  out.write_element_begin("rect");
  out.write_attribute("x", transform_x(rect.left, context));
  out.write_attribute("y", transform_y(rect.top, context));
  out.write_attribute("width", transform_x(rect.right, context) -
                                   transform_x(rect.left, context));
  out.write_attribute("height", transform_y(rect.bottom, context) -
                                    transform_y(rect.top, context));
  if (horizontal_round != 0) {
    out.write_attribute(
        "rx",
        transform_width(static_cast<std::int32_t>(horizontal_round), context));
  }
  if (vertical_round != 0) {
    out.write_attribute(
        "ry",
        transform_height(static_cast<std::int32_t>(vertical_round), context));
  }
  write_shape_style(out, context, true);
  out.write_element_end();
}

/// `svgwriter.cxx`'s `GetPathString`: one `M`, one `L` run, `Z` where closed.
std::string
get_path_data_string(const std::span<const std::vector<IntPair>> polygons,
                     const bool close, const Context &context) {
  std::string result;

  const auto append_point = [&](const IntPair point) {
    result += svg::format_number(transform_x(point.x, context));
    result += ",";
    result += svg::format_number(transform_y(point.y, context));
  };

  for (const auto &polygon : polygons) {
    if (polygon.size() < 2) {
      continue;
    }

    if (!result.empty()) {
      result += " ";
    }
    result += "M ";
    append_point(polygon.front());
    result += " L";
    for (const IntPair point : polygon | std::views::drop(1)) {
      result += " ";
      append_point(point);
    }
    // a polyline that ends where it began is closed
    if (close || (polygon.front().x == polygon.back().x &&
                  polygon.front().y == polygon.back().y)) {
      result += " Z";
    }
  }

  return result;
}

std::vector<IntPair> get_rectangle_polygon(const Rectangle &rect) {
  return {{rect.left, rect.top},
          {rect.right, rect.top},
          {rect.right, rect.bottom},
          {rect.left, rect.bottom}};
}

/// The region's outline: the shape it came from where the file kept one, and
/// the union its bands cover where it did not.
std::string get_region_path_data(const Region &region, const Context &context) {
  if (!region.polygons.empty()) {
    return get_path_data_string(region.polygons, true, context);
  }

  std::vector<std::vector<IntPair>> polygons;
  polygons.reserve(region.rectangles.size());
  for (const Rectangle &rect : region.rectangles) {
    polygons.push_back(get_rectangle_polygon(rect));
  }
  return get_path_data_string(polygons, true, context);
}

/// A file that sets the drawing area and then intersects the same rectangle
/// asks for a group that clips nothing; that one is dropped.
void intersect_clip(std::string path_data, GraphicsState &state) {
  if (!state.clip.empty() && state.clip.back() == path_data) {
    return;
  }
  state.clip.push_back(std::move(path_data));
}

/// Reconciles the open groups with the state's clip, keeping the prefix they
/// share. Every drawing action goes through here first.
void ensure_clip(Context &context) {
  svg::SvgWriter &out = *context.out;
  const std::vector<std::string> &clip = context.state.clip;

  std::size_t common = 0;
  while (common < context.written_clip.size() && common < clip.size() &&
         context.written_clip[common] == clip[common]) {
    ++common;
  }

  while (context.written_clip.size() > common) {
    out.write_element_end();
    context.written_clip.pop_back();
  }

  for (std::size_t i = common; i < clip.size(); ++i) {
    const std::string id =
        "odr-clip-" + std::to_string(++context.element_count);

    out.write_element_begin("clipPath");
    out.write_attribute("id", id);
    out.write_element_begin("path");
    out.write_attribute("d", clip[i]);
    // holes, where the region kept the shape it was rasterised from; its
    // bands never overlap, so they union under the same rule
    out.write_attribute("clip-rule", "evenodd");
    out.write_element_end();
    out.write_element_end();

    out.write_element_begin("g");
    out.write_attribute("clip-path", "url(#" + id + ")");
    context.written_clip.push_back(clip[i]);
  }
}

/// A percent out of the file, clamped: it is drawn with, not validated.
double percent(const std::uint16_t value) {
  return std::min<std::uint16_t>(value, 100) / 100.0;
}

/// The bounds of what is about to be filled, in the drawing's own coordinates.
struct Bounds final {
  double left{};
  double top{};
  double right{};
  double bottom{};

  [[nodiscard]] double width() const { return right - left; }
  [[nodiscard]] double height() const { return bottom - top; }
  [[nodiscard]] double center_x() const { return (left + right) / 2; }
  [[nodiscard]] double center_y() const { return (top + bottom) / 2; }
};

Bounds get_bounds(const std::span<const std::vector<IntPair>> polygons,
                  const Context &context) {
  Bounds result{std::numeric_limits<double>::max(),
                std::numeric_limits<double>::max(),
                std::numeric_limits<double>::lowest(),
                std::numeric_limits<double>::lowest()};

  for (const auto &polygon : polygons) {
    for (const IntPair point : polygon) {
      const double x = transform_x(point.x, context);
      const double y = transform_y(point.y, context);
      result.left = std::min(result.left, x);
      result.top = std::min(result.top, y);
      result.right = std::max(result.right, x);
      result.bottom = std::max(result.bottom, y);
    }
  }

  return result;
}

/// A gradient end's colour, scaled by the intensity the file gives it.
std::string get_gradient_color_string(const std::uint32_t color,
                                      const std::uint16_t intensity) {
  const auto scale = [&](const std::uint32_t shift) {
    return static_cast<std::uint32_t>((color >> shift & 0xff) *
                                      percent(intensity));
  };
  return get_svg_color_string(scale(16) << 16 | scale(8) << 8 | scale(0));
}

void write_gradient_stop(svg::SvgWriter &out, const double offset,
                         const std::string &color) {
  out.write_element_begin("stop");
  out.write_attribute("offset", offset);
  out.write_attribute("stop-color", color);
  out.write_element_end();
}

/// `Gradient::GetBoundRect`: a linear ramp runs across the bounds grown so
/// that turning it still covers them, from the top of that to the bottom,
/// turned about the centre - which is `(sin, cos)` of the angle.
void write_linear_gradient(const std::string &id, const Gradient &gradient,
                           const Bounds &bounds, const Context &context) {
  svg::SvgWriter &out = *context.out;

  const double angle = gradient.angle % 3600 * std::numbers::pi / 1800;
  const double grown = bounds.height() * std::abs(std::cos(angle)) +
                       bounds.width() * std::abs(std::sin(angle));
  const double x = std::sin(angle) * grown / 2;
  const double y = std::cos(angle) * grown / 2;

  out.write_element_begin("linearGradient");
  out.write_attribute("id", id);
  out.write_attribute("gradientUnits", "userSpaceOnUse");
  out.write_attribute("x1", bounds.center_x() - x);
  out.write_attribute("y1", bounds.center_y() - y);
  out.write_attribute("x2", bounds.center_x() + x);
  out.write_attribute("y2", bounds.center_y() + y);

  const std::string start =
      get_gradient_color_string(gradient.start_color, gradient.start_intensity);
  const std::string end =
      get_gradient_color_string(gradient.end_color, gradient.end_intensity);
  const double border = percent(gradient.border);

  if (gradient.style == GRADIENT_AXIAL) {
    // `DrawLinearGradient` swaps the two for an axial ramp: the end colour is
    // what both ends of the axis get, and the start colour the middle
    write_gradient_stop(out, 0, end);
    if (border > 0) {
      write_gradient_stop(out, border / 2, end);
    }
    write_gradient_stop(out, 0.5, start);
    if (border > 0) {
      write_gradient_stop(out, 1 - border / 2, end);
    }
    write_gradient_stop(out, 1, end);
  } else {
    write_gradient_stop(out, 0, start);
    if (border > 0) {
      write_gradient_stop(out, border, start);
    }
    write_gradient_stop(out, 1, end);
  }

  out.write_element_end();
}

/// The complex styles, which vcl draws as rings shrinking towards the centre:
/// the start colour is the outside and the end colour the middle. `SQUARE`
/// and `RECT` shrink a rectangle rather than an ellipse, which svg has no
/// gradient for - they come out as the ellipse they are closest to.
void write_radial_gradient(const std::string &id, const Gradient &gradient,
                           const Bounds &bounds, const Context &context) {
  svg::SvgWriter &out = *context.out;

  const bool round = gradient.style == GRADIENT_RADIAL;
  const double radius_x = round
                              ? std::hypot(bounds.width(), bounds.height()) / 2
                              : bounds.width() * std::numbers::sqrt2 / 2;
  const double radius_y =
      round ? radius_x : bounds.height() * std::numbers::sqrt2 / 2;

  out.write_element_begin("radialGradient");
  out.write_attribute("id", id);
  out.write_attribute("gradientUnits", "userSpaceOnUse");
  out.write_attribute("cx", bounds.left +
                                bounds.width() * percent(gradient.offset_x));
  out.write_attribute("cy", bounds.top +
                                bounds.height() * percent(gradient.offset_y));
  // the circle the transform below stretches to @ref radius_x
  out.write_attribute("r", radius_y * (1 - percent(gradient.border)));
  if (radius_x != radius_y) {
    out.write_attribute(
        "gradientTransform",
        "matrix(" + svg::format_number(radius_x / radius_y) + " 0 0 1 " +
            svg::format_number(bounds.center_x() * (1 - radius_x / radius_y)) +
            " 0)");
  }

  write_gradient_stop(
      out, 0,
      get_gradient_color_string(gradient.end_color, gradient.end_intensity));
  write_gradient_stop(out, 1,
                      get_gradient_color_string(gradient.start_color,
                                                gradient.start_intensity));
  out.write_element_end();
}

/// The hatch lines, as a tile the fill repeats. vcl turns them
/// counter-clockwise, svg clockwise.
void write_hatch_pattern(const std::string &id, const Hatch &hatch,
                         const Context &context) {
  svg::SvgWriter &out = *context.out;

  const double distance =
      std::max(transform_width(hatch.distance, context), 1.0);

  out.write_element_begin("pattern");
  out.write_attribute("id", id);
  out.write_attribute("patternUnits", "userSpaceOnUse");
  out.write_attribute("width", distance);
  out.write_attribute("height", distance);
  out.write_attribute("patternTransform",
                      "rotate(" + svg::format_number(hatch.angle / -10.0) +
                          ")");

  const auto line = [&](const std::string &path_data) {
    out.write_element_begin("path");
    out.write_attribute("d", path_data);
    out.write_style("stroke", get_svg_color_string(hatch.color));
    out.write_style("vector-effect", "non-scaling-stroke");
    out.write_style("fill", "none");
    out.write_element_end();
  };

  const std::string size = svg::format_number(distance);
  line("M 0,0 L " + size + ",0");
  if (hatch.style == HATCH_DOUBLE || hatch.style == HATCH_TRIPLE) {
    line("M 0,0 L 0," + size);
  }
  if (hatch.style == HATCH_TRIPLE) {
    line("M 0,0 L " + size + "," + size);
  }

  out.write_element_end();
}

/// Fills the shape with the paint @p fill names, which is written above it.
void write_filled_path(const std::span<const std::vector<IntPair>> polygons,
                       const std::string &fill, const Context &context) {
  svg::SvgWriter &out = *context.out;

  out.write_element_begin("path");
  out.write_attribute("d", get_path_data_string(polygons, true, context));
  out.write_style("fill", fill);
  out.write_style("fill-rule", "evenodd");
  out.write_style("stroke", "none");
  out.write_element_end();
}

void write_gradient(const std::span<const std::vector<IntPair>> polygons,
                    const Gradient &gradient, Context &context) {
  const Bounds bounds = get_bounds(polygons, context);
  if (bounds.left > bounds.right) {
    return;
  }

  const std::string id =
      "odr-gradient-" + std::to_string(++context.element_count);
  if (gradient.style == GRADIENT_LINEAR || gradient.style == GRADIENT_AXIAL) {
    write_linear_gradient(id, gradient, bounds, context);
  } else {
    write_radial_gradient(id, gradient, bounds, context);
  }
  write_filled_path(polygons, "url(#" + id + ")", context);
}

void write_hatch(const std::span<const std::vector<IntPair>> polygons,
                 const Hatch &hatch, Context &context) {
  const std::string id = "odr-hatch-" + std::to_string(++context.element_count);

  write_hatch_pattern(id, hatch, context);
  write_filled_path(polygons, "url(#" + id + ")", context);
}

/// The state's own colours, drawn through: `DrawTransparent` is the fill and
/// the pen at a transparency, not a colour of its own.
void write_transparent(const std::span<const std::vector<IntPair>> polygons,
                       const std::uint16_t transparence,
                       const Context &context) {
  svg::SvgWriter &out = *context.out;

  out.write_element_begin("path");
  out.write_attribute("d", get_path_data_string(polygons, true, context));
  write_shape_style(out, context, true);
  // the shape as a whole, not its fill and its stroke separately
  out.write_style("opacity", 1 - percent(transparence));
  out.write_element_end();
}

/// One path for all of them: the fill rule only cuts holes within a path.
void write_path(const std::span<const std::vector<IntPair>> polygons,
                const bool fill, const LineInfo *line_info,
                const Context &context) {
  svg::SvgWriter &out = *context.out;

  out.write_element_begin("path");
  out.write_attribute("d", get_path_data_string(polygons, fill, context));
  write_shape_style(out, context, fill, line_info);
  out.write_element_end();
}

/// What an `x` list has to match one for one.
std::size_t count_characters(const std::string_view text) {
  return std::ranges::count_if(text, [](const char c) {
    return (static_cast<unsigned char>(c) & 0xc0) != 0x80;
  });
}

/// The dx array holds the advance from the run's start to the end of each
/// character, so character *i* starts where character *i-1* ended.
std::string get_x_list_string(const IntPair &point,
                              const std::vector<std::uint32_t> &dx_array,
                              const Context &context) {
  std::string result = svg::format_number(transform_x(point.x, context));
  if (dx_array.empty()) {
    return result;
  }

  for (const std::uint32_t dx :
       dx_array | std::views::take(dx_array.size() - 1)) {
    result += " ";
    result += svg::format_number(
        transform_x(point.x + static_cast<std::int32_t>(dx), context));
  }
  return result;
}

/// @p dx_array places the characters one by one where the file measured them;
/// @p width, from a stretch text, is the advance the whole run has to fill.
/// Nothing in the file says how big a pixel is; this is what
/// `MapUnit::MapPixel` would resolve against a device.
constexpr double assumed_dpi = 96;
/// The unit the header of every metafile in the wild uses.
constexpr double hundredth_mm_per_inch = 2540;

/// A metafile's mask is white where the bitmap does not show; an svg mask
/// keeps what is white.
void write_inverter(const Context &context) {
  svg::SvgWriter &out = *context.out;

  out.write_element_begin("filter");
  out.write_attribute("id", "odr-invert");
  out.write_attribute("color-interpolation-filters", "sRGB");
  out.write_element_begin("feColorMatrix");
  out.write_attribute("type", "matrix");
  out.write_attribute("values", "-1 0 0 0 1 0 -1 0 0 1 0 0 -1 0 1 0 0 0 1 0");
  out.write_element_end();
  out.write_element_end();
}

/// In the drawing's own coordinates.
struct BitmapBox final {
  double x{};
  double y{};
  double width{};
  double height{};
};

/// The box the whole bitmap covers - for a part action, bigger than what is
/// drawn: the source rectangle scales onto the destination and the rest is
/// clipped.
BitmapBox get_bitmap_box(const BitmapAction &action, const Context &context) {
  const IntPair &size_pixel = action.bitmap.image.size_pixel;

  IntPair size = action.size;
  if (size.x == 0 || size.y == 0) {
    // the pixel count multiplies before the division truncates: `2540 / 96`
    // is 26.458, and rounding that off per pixel loses 1.7% of the size
    const auto scale_pixel = [](const std::int32_t pixels) {
      return static_cast<std::int32_t>(pixels * hundredth_mm_per_inch /
                                       assumed_dpi);
    };
    size = {scale_pixel(size_pixel.x), scale_pixel(size_pixel.y)};
  }

  BitmapBox box{transform_x(action.point.x, context),
                transform_y(action.point.y, context),
                transform_width(size.x, context),
                transform_height(size.y, context)};

  if (action.source_size.x == 0 || action.source_size.y == 0) {
    return box;
  }

  // the source rectangle, in pixels, is what fills the box, so the bitmap
  // around it is drawn at the same scale and clipped off
  const double scale_x = box.width / action.source_size.x;
  const double scale_y = box.height / action.source_size.y;
  return {box.x - action.source_point.x * scale_x,
          box.y - action.source_point.y * scale_y, size_pixel.x * scale_x,
          size_pixel.y * scale_y};
}

/// Opens the `<image>` and leaves it open: the caller adds the attributes
/// that are its own and ends it.
void write_bitmap_image(const Image &image, const BitmapBox &box,
                        const Context &context) {
  svg::SvgWriter &out = *context.out;

  out.write_element_begin("image");
  out.write_attribute("x", box.x);
  out.write_attribute("y", box.y);
  out.write_attribute("width", box.width);
  out.write_attribute("height", box.height);
  // the box is where the file puts it, aspect ratio included
  out.write_attribute("preserveAspectRatio", "none");
  out.write_attribute("href", "data:" + image.mime_type + ";base64," +
                                  crypto::util::base64_encode(image.data));
}

void write_bitmap(const BitmapAction &action, Context &context) {
  svg::SvgWriter &out = *context.out;

  if (action.bitmap.image.data.empty()) {
    ODR_WARNING(*context.logger, "a bitmap we cannot read, drawing nothing");
    return;
  }

  const BitmapBox box = get_bitmap_box(action, context);
  std::string mask_id;

  if (!action.bitmap.mask.data.empty()) {
    if (!context.inverter_written) {
      write_inverter(context);
      context.inverter_written = true;
    }
    mask_id = "odr-mask-" + std::to_string(++context.element_count);

    out.write_element_begin("mask");
    out.write_attribute("id", mask_id);
    out.write_attribute("maskUnits", "userSpaceOnUse");
    write_bitmap_image(action.bitmap.mask, box, context);
    out.write_attribute("filter", "url(#odr-invert)");
    out.write_element_end();
    out.write_element_end();
  }

  const bool clipped = action.source_size.x != 0 && action.source_size.y != 0;
  if (clipped) {
    const std::string clip_id =
        "odr-clip-" + std::to_string(++context.element_count);
    out.write_element_begin("clipPath");
    out.write_attribute("id", clip_id);
    out.write_element_begin("rect");
    out.write_attribute("x", transform_x(action.point.x, context));
    out.write_attribute("y", transform_y(action.point.y, context));
    out.write_attribute("width", transform_width(action.size.x, context));
    out.write_attribute("height", transform_height(action.size.y, context));
    out.write_element_end();
    out.write_element_end();

    write_bitmap_image(action.bitmap.image, box, context);
    out.write_attribute("clip-path", "url(#" + clip_id + ")");
  } else {
    write_bitmap_image(action.bitmap.image, box, context);
  }

  if (!mask_id.empty()) {
    out.write_attribute("mask", "url(#" + mask_id + ")");
  }
  out.write_element_end();
}

void write_ellipse(const Rectangle &rect, const Context &context) {
  svg::SvgWriter &out = *context.out;

  const double left = transform_x(rect.left, context);
  const double top = transform_y(rect.top, context);
  const double right = transform_x(rect.right, context);
  const double bottom = transform_y(rect.bottom, context);

  out.write_element_begin("ellipse");
  out.write_attribute("cx", (left + right) / 2);
  out.write_attribute("cy", (top + bottom) / 2);
  out.write_attribute("rx", std::abs(right - left) / 2);
  out.write_attribute("ry", std::abs(bottom - top) / 2);
  write_shape_style(out, context, true);
  out.write_element_end();
}

/// What an arc draws between its two rays.
enum class ArcKind {
  arc,   ///< the curve alone
  pie,   ///< closed through the centre
  chord, ///< closed straight from end to start
};

ArcKind get_arc_kind(const std::uint16_t action_type) {
  switch (action_type) {
  case META_PIE_ACTION:
    return ArcKind::pie;
  case META_CHORD_ACTION:
    return ArcKind::chord;
  default:
    return ArcKind::arc;
  }
}

/// `ImplGetParameter`: the ellipse parameter of the ray through @p point, in
/// vcl's y-up angles.
double get_arc_parameter(const IntPair &point, const double center_x,
                         const double center_y, const double radius_x,
                         const double radius_y) {
  const double angle = std::atan2(center_y - point.y, point.x - center_x);
  return std::atan2(radius_x * std::sin(angle), radius_y * std::cos(angle));
}

/// vcl sweeps counter-clockwise on screen, which is svg's sweep flag 0.
void write_arc(const ArcAction &action, const ArcKind kind,
               const Context &context) {
  svg::SvgWriter &out = *context.out;
  const Rectangle &rect = action.rectangle;

  const double center_x = (rect.left + rect.right) / 2.0;
  const double center_y = (rect.top + rect.bottom) / 2.0;
  const double radius_x = std::abs(rect.right - rect.left) / 2.0;
  const double radius_y = std::abs(rect.bottom - rect.top) / 2.0;
  if (radius_x == 0 || radius_y == 0) {
    return;
  }

  const double start =
      get_arc_parameter(action.start, center_x, center_y, radius_x, radius_y);
  const double end =
      get_arc_parameter(action.end, center_x, center_y, radius_x, radius_y);
  // the same ray twice is the whole ellipse, not nothing
  double sweep = end - start;
  if (sweep <= 0) {
    sweep += 2 * std::numbers::pi;
  }

  const auto point = [&](const double x, const double y) {
    return svg::format_number(transform_x(
               static_cast<std::int32_t>(std::lround(x)), context)) +
           "," +
           svg::format_number(
               transform_y(static_cast<std::int32_t>(std::lround(y)), context));
  };
  const auto at = [&](const double parameter) {
    return point(center_x + radius_x * std::cos(parameter),
                 center_y - radius_y * std::sin(parameter));
  };
  const std::string radii =
      " A " +
      svg::format_number(transform_width(
          static_cast<std::int32_t>(std::lround(radius_x)), context)) +
      "," +
      svg::format_number(transform_height(
          static_cast<std::int32_t>(std::lround(radius_y)), context)) +
      " 0 0 0 ";

  std::string path = "M ";
  if (kind == ArcKind::pie) {
    path += point(center_x, center_y) + " L ";
  }
  // svg draws nothing where an arc ends where it began, so a full ellipse is
  // written as its two halves
  path += at(start) + radii + at(start + sweep / 2) + radii + at(end);
  if (kind != ArcKind::arc) {
    path += " Z";
  }

  out.write_element_begin("path");
  out.write_attribute("d", path);
  // an arc is drawn, not filled; a pie and a chord are shapes
  write_shape_style(out, context, kind != ArcKind::arc);
  out.write_element_end();
}

/// A dot of the stroke's own width - a hairline is one device pixel.
void write_point(const IntPair &point, const Context &context) {
  svg::SvgWriter &out = *context.out;

  out.write_element_begin("path");
  out.write_attribute(
      "d", "M " + svg::format_number(transform_x(point.x, context)) + "," +
               svg::format_number(transform_y(point.y, context)) + " Z");
  out.write_style("stroke-linecap", "round");
  write_shape_style(out, context, false);
  out.write_element_end();
}

void write_text(const IntPair &point, const std::string &text,
                const std::vector<std::uint32_t> &dx_array,
                const std::uint32_t width, const Context &context) {
  svg::SvgWriter &out = *context.out;
  const Font &font = context.state.font;

  out.write_element_begin("text");

  if (!dx_array.empty() && dx_array.size() == count_characters(text)) {
    out.write_attribute("x", get_x_list_string(point, dx_array, context));
  } else {
    if (!dx_array.empty()) {
      ODR_DEBUG(*context.logger, "dropping a dx array of "
                                     << dx_array.size() << " for "
                                     << count_characters(text)
                                     << " characters");
    }
    out.write_attribute("x", transform_x(point.x, context));
  }
  out.write_attribute("y", transform_y(point.y, context));

  if (width > 0) {
    // the run is drawn to fill this advance, however wide the font we get is
    out.write_attribute(
        "textLength",
        transform_width(static_cast<std::int32_t>(width), context));
    out.write_attribute("lengthAdjust", "spacingAndGlyphs");
  }

  // the orientation turns the text about its own start, in tenths of a degree
  // counter-clockwise, where svg turns clockwise
  if (font.orientation != 0) {
    out.write_attribute(
        "transform",
        "rotate(" + svg::format_number(font.orientation * -0.1) + " " +
            svg::format_number(transform_x(point.x, context)) + " " +
            svg::format_number(transform_y(point.y, context)) + ")");
  }

  write_text_style(out, context);
  out.write_text(text);
  out.write_element_end();
}

void push_state(const std::uint16_t flags, Context &context) {
  if (context.stack.size() >= max_push_depth) {
    throw MalformedSvmFile();
  }
  context.stack.push_back({context.state, flags});
}

/// Restores only what the matching `PUSH` named.
void pop_state(Context &context) {
  if (context.stack.empty()) {
    ODR_WARNING(*context.logger, "pop without a push, ignoring");
    return;
  }

  const SavedState saved = std::move(context.stack.back());
  context.stack.pop_back();

  GraphicsState &state = context.state;
  if (saved.flags & PUSH_LINECOLOR) {
    state.line_rgb = saved.state.line_rgb;
    state.line_rgb_set = saved.state.line_rgb_set;
  }
  if (saved.flags & PUSH_FILLCOLOR) {
    state.fill_rgb = saved.state.fill_rgb;
    state.fill_rgb_set = saved.state.fill_rgb_set;
  }
  if (saved.flags & PUSH_FONT) {
    state.font = saved.state.font;
    state.encoding = saved.state.encoding;
  }
  if (saved.flags & PUSH_TEXTCOLOR) {
    state.text_rgb = saved.state.text_rgb;
  }
  if (saved.flags & PUSH_MAPMODE) {
    state.map_mode = saved.state.map_mode;
  }
  if (saved.flags & PUSH_TEXTFILLCOLOR) {
    state.text_fill_rgb = saved.state.text_fill_rgb;
    state.text_fill_rgb_set = saved.state.text_fill_rgb_set;
  }
  if (saved.flags & PUSH_CLIPREGION) {
    state.clip = saved.state.clip;
  }
  if (saved.flags & PUSH_TEXTALIGN) {
    state.text_align = saved.state.text_align;
  }
  if (saved.flags & PUSH_OVERLINECOLOR) {
    state.over_line_rgb = saved.state.over_line_rgb;
  }
}

void translate_action(const ActionHeader &action_header, std::istream &in,
                      Context &context) {
  GraphicsState &state = context.state;

  switch (action_header.type) {
  case META_PUSH_ACTION:
    push_state(read_push_action(in, action_header.vl), context);
    break;
  case META_POP_ACTION:
    pop_state(context);
    break;
  case META_FILLCOLOR_ACTION:
    read_primitive(in, state.fill_rgb);
    read_primitive(in, state.fill_rgb_set);
    break;
  case META_LINECOLOR_ACTION:
    read_primitive(in, state.line_rgb);
    read_primitive(in, state.line_rgb_set);
    break;
  case META_OVERLINECOLOR_ACTION:
    read_primitive(in, state.over_line_rgb);
    break;
  case META_TEXTCOLOR_ACTION:
    read_primitive(in, state.text_rgb);
    break;
  case META_TEXTFILLCOLOR_ACTION:
    read_primitive(in, state.text_fill_rgb);
    read_primitive(in, state.text_fill_rgb_set);
    break;
  case META_FONT_ACTION:
    state.font = read_font(in);
    state.encoding = static_cast<TextEncoding>(state.font.charset);
    break;
  case META_MAPMODE_ACTION:
    state.map_mode = read_map_mode(in);
    break;
  case META_PIXEL_ACTION: {
    const PixelAction action = read_pixel_action(in);
    ensure_clip(context);
    // the action carries its own colour, and nothing else draws with it
    const std::uint32_t line_rgb = std::exchange(state.line_rgb, action.color);
    const bool line_rgb_set = std::exchange(state.line_rgb_set, true);
    write_point(action.point, context);
    state.line_rgb = line_rgb;
    state.line_rgb_set = line_rgb_set;
  } break;
  case META_POINT_ACTION: {
    const IntPair action = read_int_pair(in);
    ensure_clip(context);
    write_point(action, context);
  } break;
  case META_LINE_ACTION: {
    const LineAction action = read_line_action(in, action_header.vl);
    const std::vector<IntPair> points{action.start, action.end};
    ensure_clip(context);
    write_path({&points, 1}, false, &action.line_info, context);
  } break;
  case META_ROUNDRECT_ACTION: {
    const RoundRectangleAction action = read_round_rectangle_action(in);
    ensure_clip(context);
    write_rectangle(action.rectangle, context, action.horizontal_round,
                    action.vertical_round);
  } break;
  case META_ELLIPSE_ACTION: {
    const Rectangle action = read_rectangle(in);
    ensure_clip(context);
    write_ellipse(action, context);
  } break;
  case META_ARC_ACTION:
  case META_PIE_ACTION:
  case META_CHORD_ACTION: {
    const ArcAction action = read_arc_action(in);
    ensure_clip(context);
    write_arc(action, get_arc_kind(action_header.type), context);
  } break;
  case META_GRADIENT_ACTION: {
    const Rectangle rect = read_rectangle(in);
    const Gradient gradient = read_gradient(in);
    const std::vector<IntPair> polygon = get_rectangle_polygon(rect);
    ensure_clip(context);
    write_gradient({&polygon, 1}, gradient, context);
  } break;
  case META_GRADIENTEX_ACTION: {
    const std::vector<std::vector<IntPair>> polygons = read_poly_polygon(in);
    const Gradient gradient = read_gradient(in);
    ensure_clip(context);
    write_gradient(polygons, gradient, context);
  } break;
  case META_HATCH_ACTION: {
    const std::vector<std::vector<IntPair>> polygons = read_poly_polygon(in);
    const Hatch hatch = read_hatch(in);
    ensure_clip(context);
    write_hatch(polygons, hatch, context);
  } break;
  case META_TRANSPARENT_ACTION: {
    const std::vector<std::vector<IntPair>> polygons = read_poly_polygon(in);
    std::uint16_t transparence{};
    read_primitive(in, transparence);
    ensure_clip(context);
    write_transparent(polygons, transparence, context);
  } break;
  case META_RECT_ACTION: {
    const Rectangle action = read_rectangle(in);
    ensure_clip(context);
    write_rectangle(action, context);
  } break;
  case META_POLYLINE_ACTION: {
    const auto [points, line_info] =
        read_poly_line_action(in, action_header.vl);
    ensure_clip(context);
    write_path({&points, 1}, false, &line_info, context);
  } break;
  case META_POLYGON_ACTION: {
    const auto [points] = read_polygon_action(in, action_header.vl);
    ensure_clip(context);
    write_path({&points, 1}, true, nullptr, context);
  } break;
  case META_POLYPOLYGON_ACTION: {
    const auto [polygons] = read_poly_polygon_action(in, action_header.vl);
    ensure_clip(context);
    write_path(polygons, true, nullptr, context);
  } break;
  case META_BMP_ACTION:
  case META_BMPSCALE_ACTION:
  case META_BMPSCALEPART_ACTION:
  case META_BMPEX_ACTION:
  case META_BMPEXSCALE_ACTION:
  case META_BMPEXSCALEPART_ACTION: {
    const BitmapAction action =
        read_bitmap_action(in, action_header.type, action_header.vl);
    ensure_clip(context);
    write_bitmap(action, context);
  } break;
  case META_CLIPREGION_ACTION: {
    const auto [region, clip] = read_clip_region_action(in);
    state.clip.clear();
    if (clip && region) {
      intersect_clip(get_region_path_data(*region, context), state);
    }
  } break;
  case META_ISECTRECTCLIPREGION_ACTION: {
    const Rectangle action = read_rectangle(in);
    const std::vector<IntPair> polygon = get_rectangle_polygon(action);
    intersect_clip(get_path_data_string({&polygon, 1}, true, context), state);
  } break;
  case META_ISECTREGIONCLIPREGION_ACTION: {
    if (const std::optional<Region> region = read_region(in)) {
      intersect_clip(get_region_path_data(*region, context), state);
    }
  } break;
  case META_TEXTALIGN_ACTION:
    state.text_align = read_text_align_action(in);
    break;
  case META_TEXT_ACTION: {
    const TextAction action =
        read_text_action(in, action_header.vl, state.encoding);
    ensure_clip(context);
    write_text(action.point, action.text, {}, 0, context);
  } break;
  case META_TEXTARRAY_ACTION: {
    const TextArrayAction action =
        read_text_array_action(in, action_header.vl, state.encoding);
    ensure_clip(context);
    write_text(action.point, action.text, action.dx_array, 0, context);
  } break;
  case META_STRETCHTEXT_ACTION: {
    const StretchTextAction action =
        read_stretch_text_action(in, action_header.vl, state.encoding);
    ensure_clip(context);
    write_text(action.point, action.text, {}, action.width, context);
  } break;
  default:
    ODR_DEBUG(*context.logger,
              "unhandled action " << action_name(action_header) << ", skipping "
                                  << action_header.vl.length << " bytes");
    in.ignore(static_cast<std::streamsize>(action_header.vl.length));
    break;
  }
}

} // namespace
} // namespace odr::internal::svm

namespace odr::internal {

void svm::translate_to_svg(const SvmFile &file, std::ostream &out,
                           const Logger &logger) {
  const auto istream = file.file()->stream();
  auto &in = *istream;

  svg::SvgWriter writer(out);

  Context context;
  context.out = &writer;
  context.logger = &logger;

  const Header header = read_header(in);

  context.state.map_mode = header.map_mode;

  writer.write_element_begin("svg");
  writer.write_attribute("xmlns", "http://www.w3.org/2000/svg");
  writer.write_attribute("version", "1.1");
  writer.write_attribute("viewBox", "0 0 " + std::to_string(header.size.x) +
                                        " " + std::to_string(header.size.y));

  while (in.peek() != -1) {
    // TODO check length fields should never exceed file size (limited istream?)
    const ActionHeader action_header = read_action_header(in);
    const std::int64_t start = in.tellg();

    translate_action(action_header, in, context);

    const std::int64_t left = action_header.vl.length -
                              (static_cast<std::int64_t>(in.tellg()) - start);
    if (left > 0) {
      ODR_DEBUG(logger, "action " << action_name(action_header) << " skipping "
                                  << left << " trailing bytes");
      in.ignore(static_cast<std::streamsize>(left));
    } else if (left < 0) {
      throw MalformedSvmFile();
    }
  }

  if (!context.stack.empty()) {
    ODR_WARNING(logger, context.stack.size() << " pushes were never popped");
  }

  // whatever the last clip left open
  context.state.clip.clear();
  ensure_clip(context);

  writer.write_element_end();
}

} // namespace odr::internal
