#include <glog/logging.h>
#include <internal/svm/svm_file.h>
#include <internal/svm/svm_format.h>
#include <internal/svm/svm_to_svg.h>
#include <odr/exceptions.h>
#include <string>
#include <vector>

namespace odr::internal::svm {

namespace {
struct Context final {
  std::istream *in{};
  std::ostream *out{};
  const ActionHeader *action{};

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
  return (origin + coordinate) * (double)scale.x / scale.y;
}

double transform_x(const std::int32_t x, const Context &context) {
  return transform(x, context.map_mode.origin.x, context.map_mode.scale_x);
}

double transform_y(const std::int32_t y, const Context &context) {
  return transform(y, context.map_mode.origin.y, context.map_mode.scale_y);
}

std::string get_svg_color_string(const std::uint32_t color) {
  const uint8_t blue = (color >> 0) & 0xff;
  const uint8_t green = (color >> 8) & 0xff;
  const uint8_t red = (color >> 16) & 0xff;
  return "rgb(" + std::to_string(red) + "," + std::to_string(green) + "," +
         std::to_string(blue) + ")";
}

void write_color_style(std::ostream &out, const std::string &prefix,
                       const std::uint32_t color, const bool set) {
  if (set) {
    out << prefix << ":" << get_svg_color_string(color);
  } else {
    out << prefix << "-opacity:0";
  }
  out << ";";
}

void write_line_style(std::ostream &out, Context &context) {
  write_color_style(out, "stroke", context.line_rgb, context.line_rgb_set);
  out << "vector-effect:non-scaling-stroke;";
  out << "fill:none;";
}

void write_fill_style(std::ostream &out, Context &context) {
  write_color_style(out, "fill", context.fill_rgb, context.fill_rgb_set);
  out << "stroke:none;";
}

void write_text_style(std::ostream &out, Context &context) {
  write_color_style(out, "fill", context.text_rgb, true);
  out << "font-family:" << context.font.family_name << ";";
  out << "font-size:" << context.font.size.y << ";";
}

void write_style(std::ostream &out, Context &context, const int styles) {
  out << " style=\"";
  switch (styles) {
  case 0:
    write_line_style(out, context);
    break;
  case 1:
    write_line_style(out, context);
    write_fill_style(out, context);
    break;
  case 2:
    write_text_style(out, context);
    break;
  default:
    DLOG(WARNING) << "not implemented";
  }
  out << "\"";
}

void write_rectangle(std::ostream &out, const Rectangle &rect,
                     Context &context) {
  out << "<rect";
  out << " x=\"" << transform_x(rect.left, context) << "\"";
  out << " y=\"" << transform_y(rect.top, context) << "\"";
  out << " width=\""
      << (transform_x(rect.right, context) - transform_x(rect.left, context))
      << "\"";
  out << " height=\""
      << (transform_y(rect.bottom, context) - transform_y(rect.top, context))
      << "\"";
  write_style(out, context, 1);
  out << " />";
}

void write_polygon(std::ostream &out, const std::string &tag,
                   const std::vector<IntPair> &points, const bool fill,
                   Context &context) {
  out << "<" << tag;

  out << " points=\"";
  for (auto &&p : points) {
    out << transform_x(p.x, context) << "," << transform_y(p.y, context);
    out << " ";
  }
  out << "\"";

  if (fill) {
    write_style(out, context, 1);
  } else {
    write_style(out, context, 0);
  }

  out << " />";
}

void write_text(std::ostream &out, const IntPair &point,
                const std::string &text, Context &context) {
  out << "<text";
  out << " x=\"" << transform_x(point.x, context) << "\"";
  out << " y=\"" << transform_y(point.y, context) << "\"";
  write_style(out, context, 2);
  out << ">";
  out << text;
  out << "</text>";
}

void translate_action(const ActionHeader &action_header, std::istream &in,
                      std::ostream &out, Context &context) {
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
    context.encoding = (TextEncoding)context.font.charset;
    break;
  case META_TEXTLINE_ACTION:
    context.text_line = read_text_line_action(in, action_header.vl);
    break;
  case META_RECT_ACTION: {
    Rectangle action = read_rectangle(in);
    write_rectangle(out, action, context);
  } break;
  case META_MAPMODE_ACTION: {
    context.map_mode = read_map_mode(in);
  } break;
  case META_POLYLINE_ACTION: {
    PolyLineAction action = read_poly_line_action(in, action_header.vl);
    write_polygon(out, "polyline", action.points, false, context);
  } break;
  case META_POLYGON_ACTION: {
    PolygonAction action = read_polygon_action(in, action_header.vl);
    write_polygon(out, "polygon", action.points, true, context);
  } break;
  case META_POLYPOLYGON_ACTION: {
    PolyPolygonAction action = read_poly_polygon_action(in, action_header.vl);

    for (auto &&p : action.polygons) {
      write_polygon(out, "polygon", p, true, context);
    }
  } break;
  case META_TEXT_ACTION: {
    TextAction action =
        read_text_action(in, action_header.vl, context.encoding);
    write_text(out, action.point, action.text, context);
  } break;
  case META_TEXTARRAY_ACTION: {
    TextArrayAction action =
        read_text_array_action(in, action_header.vl, context.encoding);
    write_text(out, action.point, action.text, context);
  } break;
  case META_STRETCHTEXT_ACTION: {
    StretchTextAction action =
        read_stretch_text_action(in, action_header.vl, context.encoding);
    write_text(out, action.point, action.text, context);
  } break;
  case META_TEXTRECT_ACTION: {
    TextRectangleAction action;
    // action.read(in, action_header.vl, context.encoding);
    // TODO
  } break;
  case META_NULL_ACTION:
  case META_PUSH_ACTION:
  case META_POP_ACTION:
  case META_TEXTLANGUAGE_ACTION:
  case META_COMMENT_ACTION:
    in.ignore(action_header.vl.length);
    break;
  default:
    DLOG(WARNING) << "unhandled action_header " << action_header.type
                  << " version " << action_header.vl.version << " length "
                  << action_header.vl.length;
    in.ignore(action_header.vl.length);
    break;
  }
}
} // namespace

void Translator::svg(const SvmFile &file, std::ostream &out) {
  auto istream = file.file()->read();
  auto &in = *istream;

  Context context;
  context.in = &in;
  context.out = &out;

  Header header = read_header(in);

  context.encoding = RTL_TEXTENCODING_ASCII_US;
  context.map_mode = header.map_mode;

  out << "<svg";
  out << " xmlns=\"http://www.w3.org/2000/svg\"";
  out << " version=\"1.1\"";
  out << " viewBox=\"0 0 " << header.size.x << " " << header.size.y << "\"";
  out << ">";

  while (in.peek() != -1) {
    // TODO check length fields should never exceed file size (limited istream?)
    ActionHeader action_header = read_action_header(in);
    std::int64_t start = in.tellg();

    translate_action(action_header, in, out, context);

    const std::int64_t left =
        action_header.vl.length - ((std::int64_t)in.tellg() - start);
    if (left > 0) {
      DLOG(WARNING) << "skipping " << left << " bytes of action "
                    << action_header.type << " version "
                    << action_header.vl.version;
      in.ignore(left);
    } else if (left < 0) {
      LOG(ERROR) << -left << " bytes missing action " << action_header.type
                 << " version " << action_header.vl.version;
      throw MalformedSvmFile();
    }
  }

  out << "</svg>";
}

} // namespace odr::internal::svm
