#include <odr/internal/pdf/pdf_color.hpp>

#include <odr/internal/pdf/pdf_object.hpp>
#include <odr/internal/util/color_util.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace odr::internal::pdf {

namespace {

double clamp01(const double v) {
  return std::isnan(v) ? 0.0 : std::clamp(v, 0.0, 1.0);
}

double linear_to_srgb(const double c) {
  return util::color::linear_to_srgb(clamp01(c));
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

bool read_numbers(const Object &object, const std::span<double> values,
                  const ColorSpaceContext &context) {
  const Object resolved = context.resolve(object);
  if (!resolved.is_array() || resolved.as_array().size() != values.size()) {
    return false;
  }
  for (std::size_t i = 0; i < values.size(); ++i) {
    const Object item = context.resolve(resolved.as_array()[i]);
    if (!item.is_real() || !std::isfinite(item.as_real())) {
      return false;
    }
    values[i] = item.as_real();
  }
  return true;
}

bool read_ranges(const Object &object, const std::span<double> values,
                 const ColorSpaceContext &context) {
  if (!read_numbers(object, values, context)) {
    return false;
  }
  for (std::size_t i = 0; i < values.size(); i += 2) {
    if (values[i] > values[i + 1]) {
      return false;
    }
  }
  return true;
}

/// Resolve a colour-space name to a device/pattern space, or a resource space
/// via `context.named`.
std::shared_ptr<ColorSpaceDef> space_from_name(const std::string &name,
                                               const ColorSpaceContext &ctx,
                                               const std::uint32_t depth) {
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
    return ctx.named(name, depth + 1);
  }
  return nullptr;
}

} // namespace

std::array<double, 2>
ColorSpaceDef::component_range(const std::size_t index) const {
  if (kind == ColorSpaceKind::lab && index < 3) {
    return index == 0 ? std::array<double, 2>{0, 100}
                      : std::array<double, 2>{lab_range[2 * index - 2],
                                              lab_range[2 * index - 1]};
  }
  if (kind == ColorSpaceKind::icc_based && index < 4) {
    return {icc_range[2 * index], icc_range[2 * index + 1]};
  }
  return {0, 1};
}

std::array<double, 3>
ColorSpaceDef::to_rgb(const std::span<const double> c) const {
  const auto at = [&](const std::size_t i) {
    const auto [minimum, maximum] = component_range(i);
    const double value = i < c.size() ? c[i] : 0.0;
    return kind == ColorSpaceKind::indexed
               ? value
               : std::clamp(std::isnan(value) ? 0.0 : value, minimum, maximum);
  };
  switch (kind) {
  case ColorSpaceKind::device_gray:
  case ColorSpaceKind::cal_gray: {
    const double g = at(0);
    return {g, g, g};
  }
  case ColorSpaceKind::device_rgb:
  case ColorSpaceKind::cal_rgb:
    return {at(0), at(1), at(2)};
  case ColorSpaceKind::device_cmyk:
    return cmyk_to_rgb(at(0), at(1), at(2), at(3));
  case ColorSpaceKind::lab:
    return lab_to_rgb(at(0), at(1), at(2), white_point);
  case ColorSpaceKind::icc_based:
    if (components != 1 && components != 3 && components != 4) {
      return {0, 0, 0};
    }
    if (alternate != nullptr) {
      const std::array clipped{at(0), at(1), at(2), at(3)};
      return alternate->to_rgb(
          std::span(clipped).first(static_cast<std::size_t>(components)));
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
    if (n <= 0 || hival < 0 || hival > 255 || !std::isfinite(at(0))) {
      return {0, 0, 0};
    }
    const double index = std::clamp(at(0), 0.0, static_cast<double>(hival));
    const auto offset = static_cast<std::size_t>(std::lround(index)) *
                        static_cast<std::size_t>(n);
    std::vector<double> base_components(static_cast<std::size_t>(n), 0.0);
    for (std::int32_t j = 0; j < n; ++j) {
      const std::size_t k = offset + static_cast<std::size_t>(j);
      const auto [minimum, maximum] =
          base->component_range(static_cast<std::size_t>(j));
      const double fraction = k < lookup.size()
                                  ? static_cast<std::uint8_t>(lookup[k]) / 255.0
                                  : 0.0;
      base_components[static_cast<std::size_t>(j)] =
          std::lerp(minimum, maximum, fraction);
    }
    return base->to_rgb(base_components);
  }
  case ColorSpaceKind::separation:
  case ColorSpaceKind::device_n: {
    // Without a usable tint transform, approximate a Separation as additive
    // ink over white (1 = full colorant -> black).
    const auto ink = [&]() -> std::array<double, 3> {
      const double v = clamp01(1 - at(0));
      return {v, v, v};
    };
    if (tint == nullptr || alternate == nullptr) {
      return ink();
    }
    try {
      return alternate->to_rgb(tint->eval({c.begin(), c.end()}));
    } catch (const std::invalid_argument &) {
      return ink();
    } catch (const std::runtime_error &) {
      return ink();
    }
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
  case ColorSpaceKind::lab:
  case ColorSpaceKind::icc_based: {
    std::vector<double> result(static_cast<std::size_t>(components));
    for (std::size_t i = 0; i < result.size(); ++i) {
      const auto [minimum, maximum] = component_range(i);
      result[i] = std::clamp(0.0, minimum, maximum);
    }
    return result;
  }
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

std::array<double, 3> pdf::cmyk_to_rgb(const double c, const double m,
                                       const double y, const double k) {
  const double r =
      255 +
      c * (-4.387332384609988 * c + 54.48615194189176 * m +
           18.82290502165302 * y + 212.25662451639585 * k - 285.2331026137004) +
      m * (1.7149763477362134 * m - 5.6096736904047315 * y -
           17.873870861415444 * k - 5.497006427196366) +
      y * (-2.5217340131683033 * y - 21.248923337353073 * k +
           17.5119270841813) +
      k * (-21.86122147463605 * k - 189.48180835922747);
  const double g =
      255 +
      c * (8.841041422036149 * c + 60.118027045597366 * m +
           6.871425592049007 * y + 31.159100130055922 * k - 79.2970844816548) +
      m * (-15.310361306967817 * m + 17.575251261109482 * y +
           131.35250912493976 * k - 190.9453302588951) +
      y * (4.444339102852739 * y + 9.8632861493405 * k - 24.86741582555878) +
      k * (-20.737325471181034 * k - 187.80453709719578);
  const double b = 255 +
                   c * (0.8842522430003296 * c + 8.078677503112928 * m +
                        30.89978309703729 * y - 0.23883238689178934 * k -
                        14.183576799673286) +
                   m * (10.49593273432072 * m + 63.02378494754052 * y +
                        50.606957656360734 * k - 112.23884253719248) +
                   y * (0.03296041114873217 * y + 115.60384449646641 * k -
                        193.58209356861505) +
                   k * (-22.33816807309886 * k - 180.12613974708367);
  return {pdf::clamp01(r / 255.0), pdf::clamp01(g / 255.0),
          pdf::clamp01(b / 255.0)};
}

std::shared_ptr<pdf::ColorSpaceDef>
pdf::parse_color_space(const Object &object, const ColorSpaceContext &context,
                       const std::uint32_t depth) {
  if (depth >= 64) {
    return nullptr;
  }
  const Object resolved = context.resolve(object);

  if (resolved.is_name()) {
    return space_from_name(resolved.as_name(), context, depth);
  }
  if (!resolved.is_array() || resolved.as_array().empty()) {
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
    if (!stream_dict.is_dictionary()) {
      return nullptr;
    }
    const Dictionary &dict = stream_dict.as_dictionary();
    const Object count = context.resolve(dict.get("N"));
    if (!count.is_integer() ||
        (count.as_integer() != 1 && count.as_integer() != 3 &&
         count.as_integer() != 4)) {
      return nullptr;
    }
    def->components = static_cast<std::int32_t>(count.as_integer());
    // an invalid optional Range keeps the default 0 to 1 per component
    if (std::array<double, 8> range = def->icc_range;
        dict.has_value("Range") &&
        read_ranges(dict.get("Range"),
                    std::span(range).first(
                        2 * static_cast<std::size_t>(def->components)),
                    context)) {
      def->icc_range = range;
    }
    if (dict.has_value("Alternate")) {
      def->alternate =
          parse_color_space(dict.get("Alternate"), context, depth + 1);
      // an alternate that cannot stand in leaves the device space `N` names
      if (def->alternate != nullptr &&
          (def->alternate->kind == ColorSpaceKind::pattern ||
           def->alternate->components != def->components)) {
        def->alternate = nullptr;
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
    const Object params =
        array.size() >= 2 ? context.resolve(array[1]) : Object{};
    if (!params.is_dictionary()) {
      return def;
    }
    const Dictionary &dict = params.as_dictionary();
    // an invalid WhitePoint or Range keeps the default, as a missing one does
    if (std::array<double, 3> point{};
        read_numbers(dict.get("WhitePoint"), point, context) && point[0] > 0 &&
        point[1] == 1 && point[2] > 0) {
      def->white_point = point;
    }
    if (std::array<double, 4> range{};
        dict.has_value("Range") &&
        read_ranges(dict.get("Range"), range, context)) {
      def->lab_range = range;
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
    def->base = parse_color_space(array[1], context, depth + 1);
    const Object high = context.resolve(array[2]);
    if (def->base == nullptr || def->base->kind == ColorSpaceKind::indexed ||
        def->base->kind == ColorSpaceKind::pattern || !high.is_integer() ||
        high.as_integer() < 0 || high.as_integer() > 255) {
      return nullptr;
    }
    def->hival = static_cast<std::int32_t>(high.as_integer());
    const Object lookup = context.resolve(array[3]);
    if (lookup.is_string()) {
      def->lookup = lookup.as_string();
    } else {
      def->lookup = context.load_stream(array[3]);
    }
    // a short palette reads its missing bytes as 0
    return def;
  }
  if (family == "Separation" || family == "DeviceN") {
    if (array.size() < 4) {
      return nullptr;
    }
    auto def = std::make_shared<ColorSpaceDef>();
    const Object names = context.resolve(array[1]);
    if (family == "Separation") {
      if (!names.is_name()) {
        return nullptr;
      }
      def->kind = ColorSpaceKind::separation;
      def->components = 1;
    } else {
      if (!names.is_array() || names.as_array().empty() ||
          names.as_array().size() >
              static_cast<std::size_t>(
                  std::numeric_limits<std::int32_t>::max())) {
        return nullptr;
      }
      for (const Object &name : names.as_array()) {
        if (!context.resolve(name).is_name()) {
          return nullptr;
        }
      }
      def->kind = ColorSpaceKind::device_n;
      def->components = static_cast<std::int32_t>(names.as_array().size());
    }
    def->alternate = parse_color_space(array[2], context, depth + 1);
    if (def->alternate == nullptr) {
      return nullptr;
    }
    def->tint = parse_function(
        array[3], FunctionContext{context.resolve, context.load_stream});
    return def;
  }
  if (family == "Pattern") {
    auto def = device_space(ColorSpaceKind::pattern, 1);
    if (array.size() >= 2) {
      def->base = parse_color_space(array[1], context, depth + 1);
      if (def->base == nullptr || def->base->kind == ColorSpaceKind::pattern) {
        return nullptr;
      }
      def->components = def->base->components;
    }
    return def;
  }

  return nullptr;
}

} // namespace odr::internal
