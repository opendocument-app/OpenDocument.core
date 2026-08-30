#include <odr/internal/odf/odf_geometry.hpp>

#include <odr/document_element.hpp>

#include <odr/internal/util/math_util.hpp>

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <string>

namespace odr::internal::odf {

namespace {

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

/// Composes the operation list, holding the translation in centimetres. The
/// remaining input bounds every read, so nothing here depends on a terminator.
class TransformParser {
public:
  explicit TransformParser(const std::string_view value) : m_rest{value} {}

  [[nodiscard]] std::optional<DrawingTransform> parse() {
    while (true) {
      skip_separators();
      if (m_rest.empty()) {
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
  static bool is_separator(const char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ',';
  }
  static bool is_letter(const char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
  }
  /// A superset of what a number is made of, to bound the run `std::strtod`
  /// then reads properly.
  static bool is_number_char(const char c) {
    return (c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.' ||
           c == 'e' || c == 'E';
  }

  std::string_view m_rest;

  util::math::Transform2D m_transform;
  /// What every length agreed on, or `cm` where they did not; empty until one
  /// is seen, which keeps a list of pure rotations unitless.
  std::string m_unit;

  /// The next character, or `\0` where the input ended.
  [[nodiscard]] char peek() const {
    return m_rest.empty() ? '\0' : m_rest.front();
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

  void skip_separators() {
    while (is_separator(peek())) {
      m_rest.remove_prefix(1);
    }
  }

  [[nodiscard]] std::string_view read_name() { return take_while(is_letter); }

  [[nodiscard]] bool consume(const char c) {
    skip_separators();
    if (peek() != c) {
      return false;
    }
    m_rest.remove_prefix(1);
    return true;
  }

  /// `std::strtod` wants a terminator, so the run the view bounds is copied
  /// out rather than read in place: libc++ has no floating-point
  /// `std::from_chars` until llvm 20, which the ndk, emscripten and xcode 16
  /// are all short of.
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

} // namespace odr::internal
