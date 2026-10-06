#pragma once

#include <odr/internal/pdf/pdf_function.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace odr::internal::pdf {

class Object;

/// PDF colour-space families (ISO 32000-1 8.6).
enum class ColorSpaceKind {
  device_gray,
  device_rgb,
  device_cmyk,
  cal_gray,
  cal_rgb,
  lab,
  icc_based,
  indexed,
  separation,
  device_n,
  pattern,
  unknown,
};

/// Resolved colour space for sRGB conversion (ISO 32000-1 8.6).
/// ICC uses an alternate; Cal* uses the corresponding device approximation.
struct ColorSpaceDef {
  ColorSpaceKind kind{ColorSpaceKind::unknown};
  /// Number of input components a colour in this space carries.
  std::int32_t components{1};

  // Lab (8.6.5.4): the white point and the a*/b* component ranges.
  std::array<double, 3> white_point{0.9505, 1.0, 1.089};
  std::array<double, 4> lab_range{-100, 100, -100, 100};
  std::array<double, 8> icc_range{0, 1, 0, 1, 0, 1, 0, 1};

  /// Valid component range, also used by Indexed palettes and image decoding.
  [[nodiscard]] std::array<double, 2> component_range(std::size_t index) const;

  // Indexed (8.6.6.3): the base space, the packed palette and the max index.
  std::shared_ptr<ColorSpaceDef> base;
  std::string lookup;
  std::int32_t hival{0};

  // Separation / DeviceN (8.6.6.4): the alternate space and the tint transform.
  std::shared_ptr<ColorSpaceDef> alternate;
  std::shared_ptr<Function> tint;

  /// Convert `components` of this space to sRGB in [0, 1]. A short/empty input
  /// yields the space's default colour.
  [[nodiscard]] std::array<double, 3>
  to_rgb(std::span<const double> components) const;

  /// The initial colour value of the space (ISO 32000-1 8.6.3): all-zero
  /// components, except Indexed (index 0) and Separation/DeviceN (tint 1.0).
  [[nodiscard]] std::vector<double> initial_components() const;
};

/// Resolve indirect objects, decode stream bytes and look up resource names.
/// Named lookups must forward the recursion depth.
struct ColorSpaceContext {
  std::function<Object(const Object &)> resolve;
  std::function<std::string(const Object &)> load_stream;
  std::function<std::shared_ptr<ColorSpaceDef>(const std::string &,
                                               std::uint32_t)>
      named;
};

/// DeviceCMYK -> sRGB without an ICC engine: pdf.js's polynomial fit of Adobe's
/// transform. The naive `(1-c)(1-k)` reads pure cyan as `#00ffff`.
std::array<double, 3> cmyk_to_rgb(double c, double m, double y, double k);

/// Parse a colour-space name or array; return null for unsupported, malformed
/// or excessively nested definitions.
std::shared_ptr<ColorSpaceDef>
parse_color_space(const Object &object, const ColorSpaceContext &context,
                  std::uint32_t depth = 0);

} // namespace odr::internal::pdf
