#include <odr/internal/util/color_util.hpp>

#include <algorithm>
#include <cmath>

namespace odr::internal {

std::uint8_t util::color::to_byte(const double c) noexcept {
  return static_cast<std::uint8_t>(std::lround(std::clamp(c, 0.0, 1.0) * 255));
}

double util::color::srgb_to_linear(const double c) noexcept {
  return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}

double util::color::linear_to_srgb(const double c) noexcept {
  return c <= 0.0031308 ? c * 12.92 : 1.055 * std::pow(c, 1.0 / 2.4) - 0.055;
}

util::color::Hsl util::color::Hsl::from_color(const Color &color) noexcept {
  const double r = color.red / 255.0;
  const double g = color.green / 255.0;
  const double b = color.blue / 255.0;
  const double max = std::max({r, g, b});
  const double min = std::min({r, g, b});
  Hsl result;
  result.lightness = (max + min) / 2;
  if (max == min) {
    return result;
  }
  const double d = max - min;
  result.saturation =
      result.lightness > 0.5 ? d / (2 - max - min) : d / (max + min);
  if (max == r) {
    result.hue = (g - b) / d + (g < b ? 6 : 0);
  } else if (max == g) {
    result.hue = (b - r) / d + 2;
  } else {
    result.hue = (r - g) / d + 4;
  }
  result.hue /= 6;
  return result;
}

Color util::color::Hsl::to_color(const std::uint8_t alpha) const noexcept {
  const double q = lightness < 0.5
                       ? lightness * (1 + saturation)
                       : lightness + saturation - lightness * saturation;
  const double p = 2 * lightness - q;
  const auto channel = [&](double t) {
    t -= std::floor(t);
    if (t < 1.0 / 6) {
      return to_byte(p + (q - p) * 6 * t);
    }
    if (t < 1.0 / 2) {
      return to_byte(q);
    }
    if (t < 2.0 / 3) {
      return to_byte(p + (q - p) * (2.0 / 3 - t) * 6);
    }
    return to_byte(p);
  };
  return {channel(hue + 1.0 / 3), channel(hue), channel(hue - 1.0 / 3), alpha};
}

util::color::Oklab util::color::Oklab::from_color(const Color &color) noexcept {
  const double r = srgb_to_linear(color.red / 255.0);
  const double g = srgb_to_linear(color.green / 255.0);
  const double b = srgb_to_linear(color.blue / 255.0);
  const double l =
      std::cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b);
  const double m =
      std::cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b);
  const double s =
      std::cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b);
  return {0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s,
          1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
          0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s};
}

std::array<double, 3> util::color::Oklab::to_linear_rgb() const noexcept {
  const double l_ = std::pow(l + 0.3963377774 * a + 0.2158037573 * b, 3);
  const double m_ = std::pow(l - 0.1055613458 * a - 0.0638541728 * b, 3);
  const double s_ = std::pow(l - 0.0894841775 * a - 1.2914855480 * b, 3);
  return {4.0767416621 * l_ - 3.3077115913 * m_ + 0.2309699292 * s_,
          -1.2684380046 * l_ + 2.6097574011 * m_ - 0.3413193965 * s_,
          -0.0041960863 * l_ - 0.7034186147 * m_ + 1.7076147010 * s_};
}

} // namespace odr::internal
