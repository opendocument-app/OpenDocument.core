#include <odr/internal/odf/odf_enhanced_geometry.hpp>

#include <odr/internal/odf/odf_scanner.hpp>
#include <odr/internal/util/number_util.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <system_error>

namespace odr::internal::odf {

namespace {

/// Recursive descent over 20.36's grammar: sums of products of unary terms,
/// with `$N` modifiers, `?name` equations, named values and functions.
class FormulaParser : private Scanner {
public:
  FormulaParser(const std::string_view formula,
                const EnhancedGeometryContext &context,
                const EquationResolver &equations)
      : Scanner{formula}, m_context{&context}, m_equations{&equations} {}

  [[nodiscard]] std::optional<double> parse() {
    const std::optional<double> value = expression();
    skip_space();
    if (!value.has_value() || !empty()) {
      return {};
    }
    return value;
  }

private:
  const EnhancedGeometryContext *m_context{nullptr};
  const EquationResolver *m_equations{nullptr};

  [[nodiscard]] std::optional<double> expression() {
    std::optional<double> result = term();
    while (result.has_value()) {
      skip_space();
      const char op = peek();
      if (op != '+' && op != '-') {
        break;
      }
      take();
      const std::optional<double> rhs = term();
      if (!rhs.has_value()) {
        return {};
      }
      result = op == '+' ? *result + *rhs : *result - *rhs;
    }
    return result;
  }

  [[nodiscard]] std::optional<double> term() {
    std::optional<double> result = unary();
    while (result.has_value()) {
      skip_space();
      const char op = peek();
      if (op != '*' && op != '/') {
        break;
      }
      take();
      const std::optional<double> rhs = unary();
      if (!rhs.has_value()) {
        return {};
      }
      if (op == '/' && *rhs == 0) {
        return {};
      }
      result = op == '*' ? *result * *rhs : *result / *rhs;
    }
    return result;
  }

  [[nodiscard]] std::optional<double> unary() {
    skip_space();
    if (peek() == '-') {
      take();
      const std::optional<double> value = unary();
      return value.has_value() ? std::optional<double>(-*value) : std::nullopt;
    }
    if (peek() == '+') {
      take();
      return unary();
    }
    return primary();
  }

  [[nodiscard]] std::optional<double> primary() {
    skip_space();

    if (peek() == '(') {
      take();
      const std::optional<double> value = expression();
      if (!value.has_value() || !consume(')')) {
        return {};
      }
      return value;
    }

    if (peek() == '$') {
      take();
      const std::string_view digits = take_while(is_digit);
      std::size_t index = 0;
      const std::from_chars_result read =
          std::from_chars(digits.data(), digits.data() + digits.size(), index);
      if (read.ec != std::errc() || read.ptr != digits.data() + digits.size() ||
          index >= m_context->modifiers.size()) {
        return {};
      }
      return m_context->modifiers[index];
    }

    if (peek() == '?') {
      take();
      const std::string_view name = take_while(is_letter_or_digit);
      if (name.empty()) {
        return {};
      }
      return (*m_equations)(name);
    }

    if (peek() == '.' || is_digit(peek())) {
      return read_number();
    }

    const std::string_view name = take_while(is_letter_or_digit);
    if (name.empty()) {
      return {};
    }
    skip_space();
    return peek() == '(' ? function(name) : named(name);
  }

  [[nodiscard]] std::optional<double> named(const std::string_view name) const {
    if (name == "pi") {
      return std::numbers::pi;
    }
    if (name == "left") {
      return m_context->left;
    }
    if (name == "top") {
      return m_context->top;
    }
    if (name == "right") {
      return m_context->right;
    }
    if (name == "bottom") {
      return m_context->bottom;
    }
    if (name == "width") {
      return m_context->right - m_context->left;
    }
    if (name == "height") {
      return m_context->bottom - m_context->top;
    }
    if (name == "logwidth") {
      return m_context->logical_width;
    }
    if (name == "logheight") {
      return m_context->logical_height;
    }
    if (name == "xstretch") {
      return m_context->x_stretch;
    }
    if (name == "ystretch") {
      return m_context->y_stretch;
    }
    if (name == "hasstroke") {
      return m_context->has_stroke ? 1 : 0;
    }
    if (name == "hasfill") {
      return m_context->has_fill ? 1 : 0;
    }
    return {};
  }

  [[nodiscard]] std::optional<double> function(const std::string_view name) {
    if (!consume('(')) {
      return {};
    }
    std::array<double, 3> arguments{};
    std::size_t count = 0;
    while (true) {
      const std::optional<double> argument = expression();
      if (!argument.has_value() || count >= arguments.size()) {
        return {};
      }
      arguments[count++] = *argument;
      if (!consume(',')) {
        break;
      }
    }
    if (!consume(')')) {
      return {};
    }

    if (count == 1) {
      if (name == "abs") {
        return std::abs(arguments[0]);
      }
      if (name == "sqrt") {
        return arguments[0] < 0 ? std::nullopt
                                : std::optional(std::sqrt(arguments[0]));
      }
      if (name == "sin") {
        return std::sin(arguments[0]);
      }
      if (name == "cos") {
        return std::cos(arguments[0]);
      }
      if (name == "tan") {
        return std::tan(arguments[0]);
      }
      if (name == "atan") {
        return std::atan(arguments[0]);
      }
    } else if (count == 2) {
      if (name == "min") {
        return std::min(arguments[0], arguments[1]);
      }
      if (name == "max") {
        return std::max(arguments[0], arguments[1]);
      }
      if (name == "atan2") {
        return std::atan2(arguments[0], arguments[1]);
      }
    } else if (count == 3 && name == "if") {
      return arguments[0] > 0 ? arguments[1] : arguments[2];
    }
    return {};
  }
};

/// Reads 19.145's commands and writes the svg `d` they trace.
class EnhancedPathParser : private Scanner {
public:
  EnhancedPathParser(const std::string_view path,
                     const EnhancedGeometryContext &context,
                     const EquationResolver &equations)
      : Scanner{path}, m_context{&context}, m_equations{&equations} {}

  [[nodiscard]] std::optional<std::string> parse() {
    while (true) {
      skip_separators();
      if (empty()) {
        break;
      }
      if (!parse_command()) {
        return {};
      }
    }
    if (m_out.empty()) {
      return {};
    }
    return m_out;
  }

private:
  const EnhancedGeometryContext *m_context{nullptr};
  const EquationResolver *m_equations{nullptr};

  std::string m_out;
  double m_x{0};
  double m_y{0};

  /// A number, a `$N` modifier or a `?name` equation; only the last two need
  /// the formula machinery.
  [[nodiscard]] std::optional<double> read_value() {
    skip_separators();
    if (peek() == '$' || peek() == '?') {
      const char kind = take();
      const std::string_view name = take_while(is_letter_or_digit);
      if (name.empty()) {
        return {};
      }
      return evaluate_formula(std::string(1, kind) + std::string(name),
                              *m_context, *m_equations);
    }
    return read_number();
  }

  [[nodiscard]] bool peek_value() const {
    return peek() == '$' || peek() == '?' || starts_number();
  }

  void write(const char command) {
    if (!m_out.empty()) {
      m_out += ' ';
    }
    m_out += command;
  }

  void write(const double value) {
    m_out += ' ';
    // A cancelled sine or cosine lands on negative zero, which prints as `-0`.
    m_out += util::number::to_string_significant(value == 0 ? 0 : value, 7);
  }

  /// The pen tracks the unmirrored geometry; only what is written is
  /// reflected, so the angles and radii above are computed once.
  void write_point(const double x, const double y) {
    write(m_context->mirror_horizontal ? m_context->left + m_context->right - x
                                       : x);
    write(m_context->mirror_vertical ? m_context->top + m_context->bottom - y
                                     : y);
    m_x = x;
    m_y = y;
  }

  /// A reflection in one axis reverses the way an arc turns; in both, it does
  /// not.
  void write_sweep(const bool clockwise) {
    m_out += " 0 0 ";
    m_out += (clockwise !=
              (m_context->mirror_horizontal != m_context->mirror_vertical))
                 ? '1'
                 : '0';
  }

  /// An elliptical arc from @p from to @p to degrees, in segments of at most
  /// a half turn, so a full turn — which one `A` cannot express — still draws.
  void write_arc(const double cx, const double cy, const double rx,
                 const double ry, const double from, const double to) {
    const auto point = [&](const double degrees) {
      const double radians = degrees * std::numbers::pi / 180;
      return std::array<double, 2>{cx + rx * std::cos(radians),
                                   cy + ry * std::sin(radians)};
    };
    const double swept = to - from;
    const auto segments =
        static_cast<int>(std::ceil(std::abs(swept) / 180.0 - 1e-9));
    for (int i = 1; i <= std::max(1, segments); ++i) {
      const std::array<double, 2> end =
          point(from + swept * i / std::max(1, segments));
      write('A');
      write(rx);
      write(ry);
      write_sweep(swept >= 0);
      write_point(end[0], end[1]);
    }
  }

  /// `A`, `B`, `W`, `V` (19.145): a box, and two points whose direction from
  /// its centre gives the start and end angles.
  bool write_box_arc(const bool move_first, const bool clockwise) {
    std::array<double, 8> values{};
    for (double &value : values) {
      const std::optional<double> read = read_value();
      if (!read.has_value()) {
        return false;
      }
      value = *read;
    }
    const double cx = (values[0] + values[2]) / 2;
    const double cy = (values[1] + values[3]) / 2;
    const double rx = std::abs(values[2] - values[0]) / 2;
    const double ry = std::abs(values[3] - values[1]) / 2;
    if (rx == 0 || ry == 0) {
      return false;
    }
    const auto angle = [&](const double x, const double y) {
      return std::atan2((y - cy) / ry, (x - cx) / rx) * 180 / std::numbers::pi;
    };
    const double from = angle(values[4], values[5]);
    double to = angle(values[6], values[7]);
    // A positive angle turns clockwise here, the y axis pointing down.
    if (clockwise) {
      while (to < from) {
        to += 360;
      }
    } else {
      while (to > from) {
        to -= 360;
      }
    }

    const double start_x = cx + rx * std::cos(from * std::numbers::pi / 180);
    const double start_y = cy + ry * std::sin(from * std::numbers::pi / 180);
    write(move_first ? 'M' : 'L');
    write_point(start_x, start_y);
    write_arc(cx, cy, rx, ry, from, to);
    return true;
  }

  /// `T`, `U` (19.145): a centre, radii and the two angles to sweep between.
  bool write_angle_ellipse(const bool move_first) {
    std::array<double, 6> values{};
    for (double &value : values) {
      const std::optional<double> read = read_value();
      if (!read.has_value()) {
        return false;
      }
      value = *read;
    }
    const double cx = values[0];
    const double cy = values[1];
    const double rx = values[2];
    const double ry = values[3];
    const double from = values[4];
    const double to = values[5];

    const double start_x = cx + rx * std::cos(from * std::numbers::pi / 180);
    const double start_y = cy + ry * std::sin(from * std::numbers::pi / 180);
    write(move_first ? 'M' : 'L');
    write_point(start_x, start_y);
    write_arc(cx, cy, rx, ry, from, to);
    return true;
  }

  /// `X`, `Y` (19.145): a quarter ellipse to the given point, leaving the
  /// current one along the x or the y axis.
  bool write_quadrant(const bool x_first) {
    const std::optional<double> x = read_value();
    const std::optional<double> y = read_value();
    if (!x.has_value() || !y.has_value()) {
      return false;
    }
    const double rx = std::abs(*x - m_x);
    const double ry = std::abs(*y - m_y);
    const bool descending = (*x - m_x) * (*y - m_y) > 0;
    write('A');
    write(rx);
    write(ry);
    write_sweep(descending == x_first);
    write_point(*x, *y);
    return true;
  }

  bool write_points(const char command, const std::size_t per_command) {
    do {
      std::array<double, 6> values{};
      for (std::size_t i = 0; i < per_command; ++i) {
        const std::optional<double> value = read_value();
        if (!value.has_value()) {
          return false;
        }
        values[i] = *value;
      }
      write(command);
      for (std::size_t i = 0; i + 1 < per_command; i += 2) {
        write_point(values[i], values[i + 1]);
      }
      skip_separators();
    } while (peek_value());
    return true;
  }

  [[nodiscard]] bool parse_command() {
    const char command = take();
    skip_separators();

    switch (command) {
    case 'M':
      return write_points('M', 2);
    case 'L':
      return write_points('L', 2);
    case 'C':
      return write_points('C', 6);
    case 'Q':
      return write_points('Q', 4);
    case 'Z':
      write('Z');
      return true;
    case 'N':
    case 'F':
    case 'S':
      return true;
    case 'T':
    case 'U':
      do {
        if (!write_angle_ellipse(command == 'U')) {
          return false;
        }
        skip_separators();
      } while (peek_value());
      return true;
    case 'A':
    case 'B':
    case 'W':
    case 'V':
      do {
        if (!write_box_arc(command == 'B' || command == 'V',
                           command == 'W' || command == 'V')) {
          return false;
        }
        skip_separators();
      } while (peek_value());
      return true;
    case 'X':
    case 'Y':
      do {
        if (!write_quadrant(command == 'X')) {
          return false;
        }
        skip_separators();
      } while (peek_value());
      return true;
    default:
      return false;
    }
  }
};

} // namespace

} // namespace odr::internal::odf

namespace odr::internal {

std::optional<double>
odf::evaluate_formula(const std::string_view formula,
                      const EnhancedGeometryContext &context,
                      const EquationResolver &equations) {
  return odf::FormulaParser(formula, context, equations).parse();
}

std::optional<std::string>
odf::convert_enhanced_path(const std::string_view path,
                           const EnhancedGeometryContext &context,
                           const EquationResolver &equations) {
  return odf::EnhancedPathParser(path, context, equations).parse();
}

} // namespace odr::internal
