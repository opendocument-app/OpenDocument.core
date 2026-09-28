#pragma once

#include <odr/style.hpp>

#include <array>
#include <cstdint>

namespace odr::internal::util::color {

/// @p c in [0, 1], clamped, as an 8-bit channel.
[[nodiscard]] std::uint8_t to_byte(double c) noexcept;

/// The sRGB transfer function (IEC 61966-2-1) and its inverse. Both keep 0 and
/// 1 in place and rise, so a clamp before or after gives the same result.
[[nodiscard]] double srgb_to_linear(double c) noexcept;
[[nodiscard]] double linear_to_srgb(double c) noexcept;

/// Hue, saturation and lightness, each in [0, 1].
struct Hsl final {
  double hue{0};
  double saturation{0};
  double lightness{0};

  [[nodiscard]] static Hsl from_color(const Color &color) noexcept;

  [[nodiscard]] Color to_color(std::uint8_t alpha) const noexcept;
};

/// A perceptual space: equal steps of `l` look equal, and `a` and `b` hold the
/// hue and the chroma.
struct Oklab final {
  double l{0};
  double a{0};
  double b{0};

  [[nodiscard]] static Oklab from_color(const Color &color) noexcept;

  /// Linear sRGB, possibly outside [0, 1].
  [[nodiscard]] std::array<double, 3> to_linear_rgb() const noexcept;
};

} // namespace odr::internal::util::color
