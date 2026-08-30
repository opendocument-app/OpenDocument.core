#include <odr/internal/svm/svm_to_svg.hpp>

#include <odr/exceptions.hpp>
#include <odr/logger.hpp>

#include <odr/internal/svg/svg_writer.hpp>
#include <odr/internal/svm/svm_file.hpp>
#include <odr/internal/svm/svm_format.hpp>

#include <cmath>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace odr::internal::svm {

namespace {

/// A file nests three or four deep; a thousand is a broken file, not a drawing.
constexpr std::size_t max_push_depth = 1024;

std::string action_name(const ActionHeader &action_header) {
  return std::string(action_type_name(action_header.type)) + "(" +
         std::to_string(action_header.type) + ")";
}

/// The drawing state an action reads and a `PUSH` saves, one field group per
/// `PushFlags` bit we model.
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
};

struct SavedState final {
  GraphicsState state;
  /// What the `PUSH` asked to have restored; the rest survives its `POP`.
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

/// A length carries no origin, only the scale.
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

/// The pen: the state's line colour, and the width, join and dashing of the
/// action's own `LineInfo` where it carries one.
void write_stroke_style(svg::SvgWriter &out, const Context &context,
                        const LineInfo *line_info) {
  const GraphicsState &state = context.state;
  write_color_style(out, "stroke", state.line_rgb, state.line_rgb_set);

  if (line_info == nullptr || is_default(*line_info)) {
    // a hairline is one device pixel wide however far the drawing is scaled
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

/// The brush. Sub-polygons of one shape are holes in it, which is what
/// `evenodd` cuts out.
void write_fill_style(svg::SvgWriter &out, const Context &context) {
  write_color_style(out, "fill", context.state.fill_rgb,
                    context.state.fill_rgb_set);
  out.write_style("fill-rule", "evenodd");
}

void write_text_style(svg::SvgWriter &out, const Context &context) {
  const GraphicsState &state = context.state;
  write_color_style(out, "fill", state.text_rgb, true);
  out.write_style("stroke", "none");
  out.write_style("font-family", state.font.family_name);
  // the size is a length in the drawing and scales with it
  out.write_style("font-size",
                  std::abs(transform_height(state.font.size.y, context)));
}

/// A shape is outlined by the pen and filled by the brush; a line is only
/// drawn.
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

/// `svgwriter.cxx`'s `GetPathString`: one `M`, the rest of the points as one
/// `L` run, closed with `Z` where the shape is closed.
std::string
get_path_data_string(const std::vector<std::vector<IntPair>> &polygons,
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
    // a polyline that ends where it began is a closed shape after all
    if (close || (polygon.front().x == polygon.back().x &&
                  polygon.front().y == polygon.back().y)) {
      result += " Z";
    }
  }

  return result;
}

/// Every polygon of a poly-polygon goes into **one** path: they are one shape,
/// and only then does the fill rule cut its holes out.
void write_path(const std::vector<std::vector<IntPair>> &polygons,
                const bool fill, const LineInfo *line_info,
                const Context &context) {
  svg::SvgWriter &out = *context.out;

  out.write_element_begin("path");
  out.write_attribute("d", get_path_data_string(polygons, fill, context));
  write_shape_style(out, context, fill, line_info);
  out.write_element_end();
}

void write_text(const IntPair &point, const std::string &text,
                const Context &context) {
  svg::SvgWriter &out = *context.out;

  out.write_element_begin("text");
  out.write_attribute("x", transform_x(point.x, context));
  out.write_attribute("y", transform_y(point.y, context));
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

/// Restores what the matching `PUSH` asked for, and leaves the rest as the
/// actions inside it left it.
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
    auto [points, line_info] = read_poly_line_action(in, action_header.vl);
    write_path({std::move(points)}, false, &line_info, context);
  } break;
  case META_POLYGON_ACTION: {
    auto [points] = read_polygon_action(in, action_header.vl);
    write_path({std::move(points)}, true, nullptr, context);
  } break;
  case META_POLYPOLYGON_ACTION: {
    auto [polygons] = read_poly_polygon_action(in, action_header.vl);
    write_path(polygons, true, nullptr, context);
  } break;
  case META_TEXT_ACTION: {
    const TextAction action =
        read_text_action(in, action_header.vl, state.encoding);
    write_text(action.point, action.text, context);
  } break;
  case META_TEXTARRAY_ACTION: {
    const TextArrayAction action =
        read_text_array_action(in, action_header.vl, state.encoding);
    write_text(action.point, action.text, context);
  } break;
  case META_STRETCHTEXT_ACTION: {
    const StretchTextAction action =
        read_stretch_text_action(in, action_header.vl, state.encoding);
    write_text(action.point, action.text, context);
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
