#include <odr/internal/pdf/pdf_color.hpp>

#include <odr/internal/pdf/pdf_object.hpp>

#include <algorithm>
#include <cmath>

namespace odr::internal::pdf {

namespace {

double clamp01(const double v) { return std::clamp(v, 0.0, 1.0); }

/// Naive DeviceCMYK -> RGB (no ICC), matching the HTML emitter's conversion.
std::array<double, 3> cmyk_to_rgb(const double c, const double m,
                                  const double y, const double k) {
  return {(1 - c) * (1 - k), (1 - m) * (1 - k), (1 - y) * (1 - k)};
}

/// sRGB gamma encode of a linear component (IEC 61966-2-1).
double linear_to_srgb(const double c) {
  const double v = clamp01(c);
  return v <= 0.0031308 ? 12.92 * v : 1.055 * std::pow(v, 1 / 2.4) - 0.055;
}

/// CIE L*a*b* -> sRGB through XYZ (ISO 32000-1 8.6.5.4), under the space's
/// white point.
std::array<double, 3> lab_to_rgb(const double l_star, const double a_star,
                                 const double b_star,
                                 const std::array<double, 3> &white) {
  const double fy = (l_star + 16) / 116;
  const double fx = fy + a_star / 500;
  const double fz = fy - b_star / 200;
  const auto g = [](const double t) {
    constexpr double d = 6.0 / 29.0;
    return t > d ? t * t * t : 3 * d * d * (t - 4.0 / 29.0);
  };
  const double x = white[0] * g(fx);
  const double y = white[1] * g(fy);
  const double z = white[2] * g(fz);
  // XYZ (D65) -> linear sRGB.
  const double r = 3.2406 * x - 1.5372 * y - 0.4986 * z;
  const double g_lin = -0.9689 * x + 1.8758 * y + 0.0415 * z;
  const double b = 0.0557 * x - 0.2040 * y + 1.0570 * z;
  return {linear_to_srgb(r), linear_to_srgb(g_lin), linear_to_srgb(b)};
}

std::shared_ptr<ColorSpaceDef> device_space(const ColorSpaceKind kind,
                                            const std::int32_t components) {
  auto def = std::make_shared<ColorSpaceDef>();
  def->kind = kind;
  def->components = components;
  return def;
}

std::vector<double> read_numbers(const Object &object) {
  std::vector<double> result;
  if (object.is_array()) {
    for (const Object &item : object.as_array()) {
      result.push_back(item.as_real());
    }
  }
  return result;
}

/// Resolve a colour-space name to a device/pattern space, or a resource space
/// via `context.named`.
std::shared_ptr<ColorSpaceDef> space_from_name(const std::string &name,
                                               const ColorSpaceContext &ctx) {
  if (name == "DeviceGray" || name == "G") {
    return device_space(ColorSpaceKind::device_gray, 1);
  }
  if (name == "DeviceRGB" || name == "RGB") {
    return device_space(ColorSpaceKind::device_rgb, 3);
  }
  if (name == "DeviceCMYK" || name == "CMYK") {
    return device_space(ColorSpaceKind::device_cmyk, 4);
  }
  if (name == "Pattern") {
    return device_space(ColorSpaceKind::pattern, 1);
  }
  if (ctx.named) {
    return ctx.named(name);
  }
  return nullptr;
}

} // namespace

std::array<double, 3>
ColorSpaceDef::to_rgb(const std::vector<double> &c) const {
  const auto at = [&](const std::size_t i) {
    return i < c.size() ? c[i] : 0.0;
  };
  switch (kind) {
  case ColorSpaceKind::device_gray:
  case ColorSpaceKind::cal_gray: {
    const double g = clamp01(at(0));
    return {g, g, g};
  }
  case ColorSpaceKind::device_rgb:
  case ColorSpaceKind::cal_rgb:
    return {clamp01(at(0)), clamp01(at(1)), clamp01(at(2))};
  case ColorSpaceKind::device_cmyk:
    return cmyk_to_rgb(at(0), at(1), at(2), at(3));
  case ColorSpaceKind::lab:
    return lab_to_rgb(at(0), at(1), at(2), white_point);
  case ColorSpaceKind::icc_based:
    // No ICC engine: defer to the alternate, else pick a device space by the
    // component count (ISO 32000-1 8.6.5.5).
    if (alternate != nullptr) {
      return alternate->to_rgb(c);
    }
    if (components == 1) {
      const double g = clamp01(at(0));
      return {g, g, g};
    }
    if (components == 4) {
      return cmyk_to_rgb(at(0), at(1), at(2), at(3));
    }
    return {clamp01(at(0)), clamp01(at(1)), clamp01(at(2))};
  case ColorSpaceKind::indexed: {
    if (base == nullptr) {
      return {0, 0, 0};
    }
    const std::int32_t n = base->components;
    const auto index = static_cast<std::int32_t>(std::lround(at(0)));
    const auto offset =
        static_cast<std::size_t>(std::clamp<std::int32_t>(index, 0, hival)) *
        static_cast<std::size_t>(n);
    std::vector<double> base_components(static_cast<std::size_t>(n), 0.0);
    for (std::int32_t j = 0; j < n; ++j) {
      const std::size_t k = offset + static_cast<std::size_t>(j);
      base_components[static_cast<std::size_t>(j)] =
          k < lookup.size() ? static_cast<std::uint8_t>(lookup[k]) / 255.0
                            : 0.0;
    }
    return base->to_rgb(base_components);
  }
  case ColorSpaceKind::separation:
  case ColorSpaceKind::device_n: {
    if (tint == nullptr || alternate == nullptr) {
      // Without the tint transform, approximate a Separation as additive ink
      // over white (1 = full colorant -> black).
      const double v = clamp01(1 - at(0));
      return {v, v, v};
    }
    return alternate->to_rgb(tint->eval(c));
  }
  case ColorSpaceKind::pattern:
    // An uncoloured pattern (`/PaintType 2`) carries its colour in the Pattern
    // space's underlying base (e.g. `[/Pattern /DeviceRGB]`); convert through
    // it. Without a base there is no device colour to convert.
    if (base != nullptr) {
      return base->to_rgb(c);
    }
    return {0, 0, 0};
  case ColorSpaceKind::unknown:
    return {0, 0, 0};
  }
  return {0, 0, 0};
}

std::vector<double> ColorSpaceDef::initial_components() const {
  switch (kind) {
  case ColorSpaceKind::separation:
  case ColorSpaceKind::device_n:
    // Initial tint is full colorant (ISO 32000-1 8.6.3).
    return std::vector<double>(static_cast<std::size_t>(components), 1.0);
  case ColorSpaceKind::device_cmyk:
    // Initial DeviceCMYK colour is black, i.e. {0, 0, 0, 1} (ISO
    // 32000-1 8.6.3).
    return {0.0, 0.0, 0.0, 1.0};
  default:
    return std::vector<double>(
        static_cast<std::size_t>(std::max<std::int32_t>(components, 1)), 0.0);
  }
}

} // namespace odr::internal::pdf

namespace odr::internal {

std::shared_ptr<pdf::ColorSpaceDef>
pdf::parse_color_space(const Object &object, const ColorSpaceContext &context) {
  const Object resolved = context.resolve(object);

  if (resolved.is_name()) {
    return space_from_name(resolved.as_name(), context);
  }
  if (!resolved.is_array() || resolved.as_array().size() == 0) {
    return nullptr;
  }

  const Array &array = resolved.as_array();
  const Object family_object = context.resolve(array[0]);
  if (!family_object.is_name()) {
    return nullptr;
  }
  const std::string &family = family_object.as_name();

  if (family == "ICCBased") {
    if (array.size() < 2) {
      return nullptr;
    }
    auto def = std::make_shared<ColorSpaceDef>();
    def->kind = ColorSpaceKind::icc_based;
    const Object stream_dict = context.resolve(array[1]);
    if (stream_dict.is_dictionary()) {
      const Dictionary &dict = stream_dict.as_dictionary();
      def->components =
          dict.get("N").is_integer()
              ? static_cast<std::int32_t>(dict.get("N").as_integer())
              : 3;
      if (dict.has_value("Alternate")) {
        def->alternate = parse_color_space(dict.get("Alternate"), context);
      }
    }
    return def;
  }
  if (family == "CalRGB") {
    return device_space(ColorSpaceKind::cal_rgb, 3);
  }
  if (family == "CalGray") {
    return device_space(ColorSpaceKind::cal_gray, 1);
  }
  if (family == "Lab") {
    auto def = std::make_shared<ColorSpaceDef>();
    def->kind = ColorSpaceKind::lab;
    def->components = 3;
    if (array.size() >= 2) {
      const Object params = context.resolve(array[1]);
      if (params.is_dictionary()) {
        const Dictionary &dict = params.as_dictionary();
        const std::vector<double> wp = read_numbers(dict.get("WhitePoint"));
        if (wp.size() == 3) {
          def->white_point = {wp[0], wp[1], wp[2]};
        }
        const std::vector<double> range = read_numbers(dict.get("Range"));
        if (range.size() == 4) {
          def->lab_range = {range[0], range[1], range[2], range[3]};
        }
      }
    }
    return def;
  }
  if (family == "Indexed" || family == "I") {
    if (array.size() < 4) {
      return nullptr;
    }
    auto def = std::make_shared<ColorSpaceDef>();
    def->kind = ColorSpaceKind::indexed;
    def->components = 1;
    def->base = parse_color_space(array[1], context);
    def->hival =
        static_cast<std::int32_t>(context.resolve(array[2]).as_integer());
    const Object lookup = context.resolve(array[3]);
    if (lookup.is_string()) {
      def->lookup = lookup.as_string();
    } else {
      def->lookup = context.load_stream(array[3]);
    }
    return def;
  }
  if (family == "Separation") {
    if (array.size() < 4) {
      return nullptr;
    }
    auto def = std::make_shared<ColorSpaceDef>();
    def->kind = ColorSpaceKind::separation;
    def->components = 1;
    def->alternate = parse_color_space(array[2], context);
    def->tint = parse_function(
        array[3], FunctionContext{context.resolve, context.load_stream});
    return def;
  }
  if (family == "DeviceN") {
    if (array.size() < 4) {
      return nullptr;
    }
    auto def = std::make_shared<ColorSpaceDef>();
    def->kind = ColorSpaceKind::device_n;
    const Object names = context.resolve(array[1]);
    def->components = names.is_array()
                          ? static_cast<std::int32_t>(names.as_array().size())
                          : 1;
    def->alternate = parse_color_space(array[2], context);
    def->tint = parse_function(
        array[3], FunctionContext{context.resolve, context.load_stream});
    return def;
  }
  if (family == "Pattern") {
    auto def = device_space(ColorSpaceKind::pattern, 1);
    if (array.size() >= 2) {
      def->base = parse_color_space(array[1], context);
    }
    return def;
  }

  return nullptr;
}

} // namespace odr::internal
