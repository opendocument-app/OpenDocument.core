#include <odr/internal/svm/svm_to_svg.hpp>

#include <odr/exceptions.hpp>
#include <odr/logger.hpp>

#include <odr/internal/svg/svg_writer.hpp>
#include <odr/internal/svm/svm_file.hpp>
#include <odr/internal/svm/svm_format.hpp>

#include <string>

namespace odr::internal::svm {

namespace {

/// Which of the graphics state's colours a shape draws with.
enum class StyleKind {
  line, ///< stroke, no fill
  fill, ///< fill, no stroke
  text, ///< the text colour as fill, plus the font
};

/// The action type's name with its number, for a log line.
std::string action_name(const ActionHeader &action_header) {
  return std::string(action_type_name(action_header.type)) + "(" +
         std::to_string(action_header.type) + ")";
}

struct Context final {
  svg::SvgWriter *out{};
  const Logger *logger{};

  MapMode map_mode;
  TextEncoding encoding{};
  Font font;
  TextLineAction text_line;
  std::uint32_t fill_rgb{};
  bool fill_rgb_set{};
  std::uint32_t line_rgb{};
  bool line_rgb_set{};
  std::uint32_t over_line_rgb{};
  std::uint32_t text_rgb{};
  std::uint32_t text_fill_rgb{};
  bool text_fill_rgb_set{};
};

double transform(const std::int32_t coordinate, const std::int32_t origin,
                 const IntPair scale) {
  return (origin + coordinate) * static_cast<double>(scale.x) / scale.y;
}

double transform_x(const std::int32_t x, const Context &context) {
  return transform(x, context.map_mode.origin.x, context.map_mode.scale_x);
}

double transform_y(const std::int32_t y, const Context &context) {
  return transform(y, context.map_mode.origin.y, context.map_mode.scale_y);
}

std::string get_svg_color_string(const std::uint32_t color) {
  const std::uint8_t blue = color >> 0 & 0xff;
  const std::uint8_t green = color >> 8 & 0xff;
  const std::uint8_t red = color >> 16 & 0xff;
  return "rgb(" + std::to_string(red) + "," + std::to_string(green) + "," +
         std::to_string(blue) + ")";
}

/// The colour, or the property turned off where the state does not set one.
void write_color_style(svg::SvgWriter &out, const std::string &property,
                       const std::uint32_t color, const bool set) {
  if (set) {
    out.write_style(property, get_svg_color_string(color));
  } else {
    out.write_style(property + "-opacity", "0");
  }
}

void write_line_style(svg::SvgWriter &out, const Context &context) {
  write_color_style(out, "stroke", context.line_rgb, context.line_rgb_set);
  out.write_style("vector-effect", "non-scaling-stroke");
  out.write_style("fill", "none");
}

void write_fill_style(svg::SvgWriter &out, const Context &context) {
  write_color_style(out, "fill", context.fill_rgb, context.fill_rgb_set);
  out.write_style("stroke", "none");
}

void write_text_style(svg::SvgWriter &out, const Context &context) {
  write_color_style(out, "fill", context.text_rgb, true);
  out.write_style("font-family", context.font.family_name);
  out.write_style("font-size", context.font.size.y);
}

void write_style(svg::SvgWriter &out, const Context &context,
                 const StyleKind kind) {
  switch (kind) {
  case StyleKind::line:
    write_line_style(out, context);
    break;
  case StyleKind::fill:
    // TODO the fill kills the stroke the line style just wrote
    write_line_style(out, context);
    write_fill_style(out, context);
    break;
  case StyleKind::text:
    write_text_style(out, context);
    break;
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
  write_style(out, context, StyleKind::fill);
  out.write_element_end();
}

void write_polygon(const std::string &tag, const std::vector<IntPair> &points,
                   const bool fill, const Context &context) {
  svg::SvgWriter &out = *context.out;

  std::string points_attribute;
  for (const auto [x, y] : points) {
    points_attribute += svg::format_number(transform_x(x, context));
    points_attribute += ",";
    points_attribute += svg::format_number(transform_y(y, context));
    points_attribute += " ";
  }

  out.write_element_begin(tag);
  out.write_attribute("points", points_attribute);
  write_style(out, context, fill ? StyleKind::fill : StyleKind::line);
  out.write_element_end();
}

void write_text(const IntPair &point, const std::string &text,
                const Context &context) {
  svg::SvgWriter &out = *context.out;

  out.write_element_begin("text");
  out.write_attribute("x", transform_x(point.x, context));
  out.write_attribute("y", transform_y(point.y, context));
  write_style(out, context, StyleKind::text);
  out.write_text(text);
  out.write_element_end();
}

void translate_action(const ActionHeader &action_header, std::istream &in,
                      Context &context) {
  switch (action_header.type) {
  case META_FILLCOLOR_ACTION:
    read_primitive(in, context.fill_rgb);
    read_primitive(in, context.fill_rgb_set);
    break;
  case META_LINECOLOR_ACTION:
    read_primitive(in, context.line_rgb);
    read_primitive(in, context.line_rgb_set);
    break;
  case META_OVERLINECOLOR_ACTION:
    read_primitive(in, context.over_line_rgb);
    break;
  case META_TEXTCOLOR_ACTION:
    read_primitive(in, context.text_rgb);
    break;
  case META_TEXTFILLCOLOR_ACTION:
    read_primitive(in, context.text_fill_rgb);
    read_primitive(in, context.text_fill_rgb_set);
    break;
  case META_FONT_ACTION:
    context.font = read_font(in);
    context.encoding = static_cast<TextEncoding>(context.font.charset);
    break;
  case META_TEXTLINE_ACTION:
    context.text_line = read_text_line_action(in, action_header.vl);
    break;
  case META_RECT_ACTION: {
    const Rectangle action = read_rectangle(in);
    write_rectangle(action, context);
  } break;
  case META_MAPMODE_ACTION: {
    context.map_mode = read_map_mode(in);
  } break;
  case META_POLYLINE_ACTION: {
    auto [points, line_info] = read_poly_line_action(in, action_header.vl);
    write_polygon("polyline", points, false, context);
  } break;
  case META_POLYGON_ACTION: {
    auto [points] = read_polygon_action(in, action_header.vl);
    write_polygon("polygon", points, true, context);
  } break;
  case META_POLYPOLYGON_ACTION: {
    auto [polygons] = read_poly_polygon_action(in, action_header.vl);
    for (const auto &p : polygons) {
      write_polygon("polygon", p, true, context);
    }
  } break;
  case META_TEXT_ACTION: {
    const TextAction action =
        read_text_action(in, action_header.vl, context.encoding);
    write_text(action.point, action.text, context);
  } break;
  case META_TEXTARRAY_ACTION: {
    const TextArrayAction action =
        read_text_array_action(in, action_header.vl, context.encoding);
    write_text(action.point, action.text, context);
  } break;
  case META_STRETCHTEXT_ACTION: {
    const StretchTextAction action =
        read_stretch_text_action(in, action_header.vl, context.encoding);
    write_text(action.point, action.text, context);
  } break;
  default:
    ODR_DEBUG(*context.logger,
              "unhandled action " << action_name(action_header) << ", skipping "
                                  << action_header.vl.length << " bytes");
    in.ignore(action_header.vl.length);
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

  context.encoding = RTL_TEXTENCODING_ASCII_US;
  context.map_mode = header.map_mode;

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

  writer.write_element_end();
}

} // namespace odr::internal
