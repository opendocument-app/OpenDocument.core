#include <odr/internal/svm/svm_to_svg.hpp>

#include <odr/exceptions.hpp>
#include <odr/logger.hpp>

#include <odr/internal/svg/svg_writer.hpp>
#include <odr/internal/svm/svm_file.hpp>
#include <odr/internal/svm/svm_format.hpp>

#include <algorithm>
#include <cmath>
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

/// Which edge of the text the draw point names. The browser knows the font's
/// metrics and we do not, so the baseline is named rather than computed -
/// `svgwriter.cxx` shifts the point by the ascent instead, having them.
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

void write_rectangle(const Rectangle &rect, const Context &context) {
  svg::SvgWriter &out = *context.out;

  out.write_element_begin("rect");
  out.write_attribute("x", transform_x(rect.left, context));
  out.write_attribute("y", transform_y(rect.top, context));
  out.write_attribute("width", transform_x(rect.right, context) -
                                   transform_x(rect.left, context));
  out.write_attribute("height", transform_y(rect.bottom, context) -
                                    transform_y(rect.top, context));
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

/// The number of characters svg will place, which is what an `x` list has to
/// match one for one.
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
  case META_RECT_ACTION: {
    const Rectangle action = read_rectangle(in);
    write_rectangle(action, context);
  } break;
  case META_POLYLINE_ACTION: {
    const auto [points, line_info] =
        read_poly_line_action(in, action_header.vl);
    write_path({&points, 1}, false, &line_info, context);
  } break;
  case META_POLYGON_ACTION: {
    const auto [points] = read_polygon_action(in, action_header.vl);
    write_path({&points, 1}, true, nullptr, context);
  } break;
  case META_POLYPOLYGON_ACTION: {
    const auto [polygons] = read_poly_polygon_action(in, action_header.vl);
    write_path(polygons, true, nullptr, context);
  } break;
  case META_TEXTALIGN_ACTION:
    state.text_align = read_text_align_action(in);
    break;
  case META_TEXT_ACTION: {
    const TextAction action =
        read_text_action(in, action_header.vl, state.encoding);
    write_text(action.point, action.text, {}, 0, context);
  } break;
  case META_TEXTARRAY_ACTION: {
    const TextArrayAction action =
        read_text_array_action(in, action_header.vl, state.encoding);
    write_text(action.point, action.text, action.dx_array, 0, context);
  } break;
  case META_STRETCHTEXT_ACTION: {
    const StretchTextAction action =
        read_stretch_text_action(in, action_header.vl, state.encoding);
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

  writer.write_element_end();
}

} // namespace odr::internal
