#include <odr/internal/odf/odf_geometry.hpp>

#include <odr/document_element.hpp>

#include <odr/internal/util/math_util.hpp>
#include <odr/internal/util/number_util.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <numbers>
#include <string>
#include <vector>

namespace odr::internal::odf {

namespace {

/// The square a shape with no view box of its own is drawn into; the size is
/// arbitrary, and this is the one `draw:enhanced-geometry` uses.
constexpr double view_box_size = 21600;
constexpr double view_box_centre = view_box_size / 2;

/// Zero for a unit that is not an absolute length.
double centimetres_per(const std::string_view unit) {
  if (unit == "cm") {
    return 1.0;
  }
  if (unit == "mm") {
    return 0.1;
  }
  if (unit == "in") {
    return 2.54;
  }
  if (unit == "pt") {
    return 2.54 / 72.0;
  }
  if (unit == "pc") {
    return 2.54 / 6.0;
  }
  if (unit == "px") {
    return 2.54 / 96.0;
  }
  return 0.0;
}

/// A cursor over the input every reader here shares. Reads are bounded by what
/// remains, which carries no terminator.
class Scanner {
public:
  explicit Scanner(const std::string_view input) : m_rest{input} {}

  [[nodiscard]] bool empty() const { return m_rest.empty(); }

  /// The next character, or `\0` where the input ended.
  [[nodiscard]] char peek() const {
    return m_rest.empty() ? '\0' : m_rest.front();
  }

  /// The next character, consumed.
  char take() {
    const char c = peek();
    if (!m_rest.empty()) {
      m_rest.remove_prefix(1);
    }
    return c;
  }

  void skip_separators() {
    while (is_separator(peek())) {
      m_rest.remove_prefix(1);
    }
  }

  [[nodiscard]] bool consume(const char c) {
    skip_separators();
    if (peek() != c) {
      return false;
    }
    m_rest.remove_prefix(1);
    return true;
  }

  /// The leading run of characters @p accept admits, left in place.
  [[nodiscard]] std::string_view peek_while(bool (*accept)(char)) const {
    std::size_t length = 0;
    while (length < m_rest.size() && accept(m_rest[length])) {
      ++length;
    }
    return m_rest.substr(0, length);
  }

  /// The same run, consumed.
  [[nodiscard]] std::string_view take_while(bool (*accept)(char)) {
    const std::string_view taken = peek_while(accept);
    m_rest.remove_prefix(taken.size());
    return taken;
  }

  /// `std::strtod` wants a terminator, which the view does not promise, so the
  /// run it bounds is copied out.
  [[nodiscard]] std::optional<double> read_number() {
    skip_separators();
    const std::string number(peek_while(is_number_char));
    char *end = nullptr;
    const double value = std::strtod(number.c_str(), &end);
    if (end == number.c_str()) {
      return {};
    }
    // `strtod` may stop short of the run, on a trailing `e` say
    m_rest.remove_prefix(static_cast<std::size_t>(end - number.c_str()));
    return value;
  }

  [[nodiscard]] bool starts_number() const {
    const char c = peek();
    return c == '-' || c == '+' || c == '.' || (c >= '0' && c <= '9');
  }

  static bool is_letter(const char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
  }

private:
  static bool is_separator(const char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ',';
  }
  /// A superset of a number's characters, to bound the run `std::strtod` reads.
  static bool is_number_char(const char c) {
    return (c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.' ||
           c == 'e' || c == 'E';
  }

  std::string_view m_rest;
};

/// Composes the operation list, holding the translation in centimetres.
class TransformParser : private Scanner {
public:
  using Scanner::Scanner;

  [[nodiscard]] std::optional<DrawingTransform> parse() {
    while (true) {
      skip_separators();
      if (empty()) {
        break;
      }
      if (!parse_operation()) {
        return {};
      }
    }

    const double scale = m_unit.empty() ? 1.0 : centimetres_per(m_unit);
    const DynamicUnit unit{m_unit};
    return DrawingTransform{
        .a = m_transform.a,
        .b = m_transform.b,
        .c = m_transform.c,
        .d = m_transform.d,
        .e = Measure(m_transform.e / scale, unit),
        .f = Measure(m_transform.f / scale, unit),
    };
  }

private:
  util::math::Transform2D m_transform;
  /// What every length agreed on, or `cm` where they did not; empty until one
  /// is seen, which keeps a list of pure rotations unitless.
  std::string m_unit;

  [[nodiscard]] std::string_view read_name() { return take_while(is_letter); }

  /// Reduced to centimetres.
  [[nodiscard]] std::optional<double> read_length() {
    const std::optional<double> value = read_number();
    if (!value.has_value()) {
      return {};
    }
    const std::string_view unit =
        take_while([](const char c) { return is_letter(c) || c == '%'; });

    // A zero needs no unit.
    if (unit.empty()) {
      return *value == 0.0 ? std::optional<double>(0.0) : std::nullopt;
    }
    const double scale = centimetres_per(unit);
    if (scale == 0.0) {
      return {};
    }
    if (m_unit.empty()) {
      m_unit = unit;
    } else if (m_unit != unit) {
      m_unit = "cm";
    }
    return *value * scale;
  }

  /// The list applies left to right, so a `translate` after a `rotate` is not
  /// itself rotated.
  void compose(const util::math::Transform2D &operation) {
    m_transform = m_transform * operation;
  }

  [[nodiscard]] bool parse_operation() {
    const std::string_view name = read_name();
    if (name.empty() || !consume('(')) {
      return false;
    }

    if (name == "matrix") {
      const std::optional<double> a = read_number();
      const std::optional<double> b = read_number();
      const std::optional<double> c = read_number();
      const std::optional<double> d = read_number();
      const std::optional<double> e = read_length();
      const std::optional<double> f = read_length();
      if (!a || !b || !c || !d || !e || !f) {
        return false;
      }
      compose({*a, *b, *c, *d, *e, *f});
    } else if (name == "translate") {
      const std::optional<double> x = read_length();
      if (!x) {
        return false;
      }
      const std::optional<double> y =
          peek_argument() ? read_length() : std::optional<double>(0);
      if (!y) {
        return false;
      }
      compose(util::math::Transform2D::translation(*x, *y));
    } else if (name == "scale") {
      const std::optional<double> x = read_number();
      if (!x) {
        return false;
      }
      const std::optional<double> y = peek_argument() ? read_number() : x;
      if (!y) {
        return false;
      }
      compose(util::math::Transform2D::scaling(*x, *y));
    } else if (name == "rotate" || name == "skewX" || name == "skewY") {
      const std::optional<double> angle = read_number();
      if (!angle) {
        return false;
      }
      // Radians, counter-clockwise, so the sine changes sign against svg's
      // `rotate` in the same y-down space; the skews take that handedness. No
      // corpus file skews measurably.
      if (name == "rotate") {
        const double sin = std::sin(*angle);
        const double cos = std::cos(*angle);
        compose({cos, -sin, sin, cos, 0, 0});
      } else if (name == "skewX") {
        compose({1, 0, -std::tan(*angle), 1, 0, 0});
      } else {
        compose({1, -std::tan(*angle), 0, 1, 0, 0});
      }
    } else {
      return false;
    }

    return consume(')');
  }

  [[nodiscard]] bool peek_argument() {
    skip_separators();
    return peek() != ')' && peek() != '\0';
  }
};

/// Reads an svg `d` (19.180) and writes it back out, boxed by every point and
/// control point it names. Only the numbers are re-rendered.
class PathParser : private Scanner {
public:
  using Scanner::Scanner;

  [[nodiscard]] std::optional<DrawingPath> parse() {
    while (true) {
      skip_separators();
      if (empty()) {
        break;
      }
      if (!parse_command()) {
        return {};
      }
    }
    if (m_empty) {
      return {};
    }
    return DrawingPath{
        .data = m_out,
        .x = m_min_x,
        .y = m_min_y,
        .width = m_max_x - m_min_x,
        .height = m_max_y - m_min_y,
    };
  }

private:
  std::string m_out;
  char m_command{'\0'};

  /// The pen, and the current subpath's start, both absolute.
  double m_x{0};
  double m_y{0};
  double m_start_x{0};
  double m_start_y{0};

  bool m_empty{true};
  double m_min_x{0};
  double m_min_y{0};
  double m_max_x{0};
  double m_max_y{0};

  void write_command(const char command) {
    if (!m_out.empty()) {
      m_out += ' ';
    }
    m_out += command;
  }

  void write_number(const double value) {
    m_out += ' ';
    m_out += util::number::to_string_significant(value, 7);
  }

  void cover(const double x, const double y) {
    if (m_empty) {
      m_min_x = m_max_x = x;
      m_min_y = m_max_y = y;
      m_empty = false;
      return;
    }
    m_min_x = std::min(m_min_x, x);
    m_min_y = std::min(m_min_y, y);
    m_max_x = std::max(m_max_x, x);
    m_max_y = std::max(m_max_y, y);
  }

  /// Numbers per repetition; `-1` for a letter that is not a command.
  [[nodiscard]] static int arity(const char command) {
    switch (std::tolower(static_cast<unsigned char>(command))) {
    case 'z':
      return 0;
    case 'h':
    case 'v':
      return 1;
    case 'm':
    case 'l':
    case 't':
      return 2;
    case 's':
    case 'q':
      return 4;
    case 'c':
      return 6;
    case 'a':
      return 7;
    default:
      return -1;
    }
  }

  /// A command letter, then as many argument sets as follow it (19.180).
  [[nodiscard]] bool parse_command() {
    if (arity(peek()) >= 0) {
      m_command = take();
    } else if (m_command == '\0') {
      return false;
    }
    do {
      if (!write_arguments()) {
        return false;
      }
      skip_separators();
    } while (arity(m_command) > 0 && starts_number());
    return true;
  }

  [[nodiscard]] bool write_arguments() {
    const char command = m_command;
    const int count = arity(command);
    const bool relative = command >= 'a' && command <= 'z';

    if (count == 0) {
      write_command(command);
      m_x = m_start_x;
      m_y = m_start_y;
      return true;
    }

    std::vector<double> arguments(static_cast<std::size_t>(count));
    for (double &argument : arguments) {
      const std::optional<double> value = read_number();
      if (!value.has_value()) {
        return false;
      }
      argument = *value;
    }

    write_command(command);
    for (const double argument : arguments) {
      write_number(argument);
    }

    const double origin_x = relative ? m_x : 0;
    const double origin_y = relative ? m_y : 0;

    switch (std::tolower(static_cast<unsigned char>(command))) {
    case 'h':
      m_x = origin_x + arguments[0];
      break;
    case 'v':
      m_y = origin_y + arguments[0];
      break;
    case 'a':
      // The radii, rotation and flags ahead of the endpoint are not points.
      m_x = origin_x + arguments[5];
      m_y = origin_y + arguments[6];
      break;
    default:
      // Coordinate pairs, the last of which is where the pen lands.
      for (std::size_t i = 0; i + 1 < arguments.size(); i += 2) {
        cover(origin_x + arguments[i], origin_y + arguments[i + 1]);
      }
      m_x = origin_x + arguments[arguments.size() - 2];
      m_y = origin_y + arguments[arguments.size() - 1];
      break;
    }
    cover(m_x, m_y);

    if (command == 'M' || command == 'm') {
      m_start_x = m_x;
      m_start_y = m_y;
      // A repeated `moveto` pair is a `lineto` (19.180).
      m_command = command == 'M' ? 'L' : 'l';
    }

    return true;
  }
};

/// The user-space box a shape's geometry is written in.
struct ViewBox {
  double x{0};
  double y{0};
  double width{0};
  double height{0};
};

/// `svg:viewBox` (19.508).
std::optional<ViewBox> read_view_box(const pugi::xml_node node) {
  const pugi::xml_attribute attribute = node.attribute("svg:viewBox");
  if (!attribute) {
    return {};
  }
  Scanner in(attribute.value());
  std::array<double, 4> values{};
  for (double &value : values) {
    const std::optional<double> number = in.read_number();
    if (!number.has_value()) {
      return {};
    }
    value = *number;
  }
  if (values[2] <= 0 || values[3] <= 0) {
    return {};
  }
  return ViewBox{
      .x = values[0], .y = values[1], .width = values[2], .height = values[3]};
}

/// `draw:points` (19.187): `x,y` pairs in the view box's coordinates.
std::optional<std::string> read_points(const pugi::xml_node node,
                                       const bool close) {
  const pugi::xml_attribute attribute = node.attribute("draw:points");
  if (!attribute) {
    return {};
  }
  Scanner in(attribute.value());

  std::string result;
  while (true) {
    in.skip_separators();
    if (in.empty()) {
      break;
    }
    const std::optional<double> x = in.read_number();
    const std::optional<double> y = in.read_number();
    if (!x.has_value() || !y.has_value()) {
      return {};
    }
    result += result.empty() ? "M " : " L ";
    result += util::number::to_string_significant(*x, 7);
    result += ' ';
    result += util::number::to_string_significant(*y, 7);
  }
  if (result.empty()) {
    return {};
  }
  if (close) {
    result += " Z";
  }
  return result;
}

/// `draw:regular-polygon` (10.3.9): `draw:corners` vertices from the top, a
/// concave one alternating with a vertex `draw:sharpness` of the way in.
std::optional<DrawingPath> read_regular_polygon(const pugi::xml_node node) {
  const int corners = node.attribute("draw:corners").as_int(0);
  if (corners < 3) {
    return {};
  }
  const bool concave = node.attribute("draw:concave").as_bool(false);
  const double sharpness =
      node.attribute("draw:sharpness").as_double(50.0) / 100.0;

  const int vertices = concave ? corners * 2 : corners;

  std::string result;
  for (int i = 0; i < vertices; ++i) {
    const double radius = (concave && i % 2 == 1)
                              ? view_box_centre * (1 - sharpness)
                              : view_box_centre;
    const double angle =
        -std::numbers::pi / 2 + 2 * std::numbers::pi * i / vertices;
    result += result.empty() ? "M " : " L ";
    result += util::number::to_string_significant(
        view_box_centre + radius * std::cos(angle), 7);
    result += ' ';
    result += util::number::to_string_significant(
        view_box_centre + radius * std::sin(angle), 7);
  }
  result += " Z";

  return DrawingPath{.data = result,
                     .x = 0,
                     .y = 0,
                     .width = view_box_size,
                     .height = view_box_size};
}

/// A `draw:circle` / `draw:ellipse` that `draw:kind` (19.212) cuts.
std::optional<DrawingPath> read_elliptical_kind(const pugi::xml_node node) {
  const std::string_view kind = node.attribute("draw:kind").value();
  if (kind != "arc" && kind != "cut" && kind != "section") {
    return {};
  }

  const double start = node.attribute("draw:start-angle").as_double(0);
  const double end = node.attribute("draw:end-angle").as_double(360);

  const auto point = [](const double degrees) {
    const double radians = degrees * std::numbers::pi / 180;
    // Counter-clockwise from the positive x axis (19.203), y down.
    return std::array<double, 2>{
        view_box_centre + view_box_centre * std::cos(radians),
        view_box_centre - view_box_centre * std::sin(radians)};
  };
  const auto write = [](const double value) {
    return util::number::to_string_significant(value, 7);
  };

  double swept = std::fmod(end - start, 360.0);
  if (swept <= 0) {
    swept += 360.0;
  }
  const std::array<double, 2> from = point(start);
  // Counter-clockwise in y-down is svg's sweep flag clear.
  const auto arc_to = [&](const std::array<double, 2> &target,
                          const bool large) {
    return " A " + write(view_box_centre) + " " + write(view_box_centre) +
           " 0 " + (large ? "1" : "0") + " 0 " + write(target[0]) + " " +
           write(target[1]);
  };

  std::string result = "M " + write(from[0]) + " " + write(from[1]);
  if (swept >= 360) {
    // Svg draws nothing for an arc ending where it starts, so a full sweep
    // goes round through the opposite point.
    result += arc_to(point(start + 180), false) + arc_to(from, false);
  } else {
    result += arc_to(point(end), swept > 180);
  }
  if (kind == "section") {
    result +=
        " L " + write(view_box_centre) + " " + write(view_box_centre) + " Z";
  } else if (kind == "cut") {
    result += " Z";
  }

  return DrawingPath{.data = result,
                     .x = 0,
                     .y = 0,
                     .width = view_box_size,
                     .height = view_box_size};
}

} // namespace

} // namespace odr::internal::odf

namespace odr::internal {

std::optional<DrawingTransform> odf::read_transform(const pugi::xml_node node) {
  const pugi::xml_attribute attribute = node.attribute("draw:transform");
  if (!attribute) {
    return {};
  }
  return parse_transform(attribute.value());
}

std::optional<DrawingTransform>
odf::parse_transform(const std::string_view value) {
  return odf::TransformParser(value).parse();
}

std::optional<DrawingPath> odf::parse_path_data(const std::string_view data) {
  return odf::PathParser(data).parse();
}

std::optional<DrawingPath> odf::read_path(const pugi::xml_node node) {
  const std::string_view name = node.name();

  if (name == "draw:path") {
    std::optional<DrawingPath> data =
        parse_path_data(node.attribute("svg:d").value());
    if (!data.has_value()) {
      return {};
    }
    // The path's own extent is only the fallback for a shape stating none.
    if (const std::optional<ViewBox> view_box = odf::read_view_box(node)) {
      data->x = view_box->x;
      data->y = view_box->y;
      data->width = view_box->width;
      data->height = view_box->height;
    }
    return data;
  }

  if (name == "draw:polygon" || name == "draw:polyline") {
    const std::optional<std::string> data =
        odf::read_points(node, name == "draw:polygon");
    const std::optional<ViewBox> view_box = odf::read_view_box(node);
    if (!data.has_value() || !view_box.has_value()) {
      return {};
    }
    return DrawingPath{.data = *data,
                       .x = view_box->x,
                       .y = view_box->y,
                       .width = view_box->width,
                       .height = view_box->height};
  }

  if (name == "draw:regular-polygon") {
    return odf::read_regular_polygon(node);
  }

  if (name == "draw:connector") {
    // A connector's `svg:d` is in the page's own coordinates, so the path is
    // its own box; one unit keeps a straight one's box legal in svg.
    std::optional<DrawingPath> path =
        parse_path_data(node.attribute("svg:d").value());
    if (path.has_value()) {
      path->width = std::max(path->width, 1.0);
      path->height = std::max(path->height, 1.0);
    }
    return path;
  }

  if (name == "draw:circle" || name == "draw:ellipse") {
    return odf::read_elliptical_kind(node);
  }

  return {};
}

} // namespace odr::internal
