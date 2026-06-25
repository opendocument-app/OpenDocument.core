#pragma once

#include <odr/internal/pdf/pdf_function.hpp>

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace odr::internal::pdf {

class Object;

/// The colour-space families PDF content can select (ISO 32000-1 8.6). The
/// device families are also reachable through the dedicated operators
/// (`g`/`rg`/`k`); the rest arrive via `/ColorSpace` resources and `cs`/`scn`.
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

/// A resolved colour space: enough to convert a tuple of component values to
/// sRGB at emit time (ISO 32000-1 8.6.4 / 8.6.5 / 8.6.6). Non-device spaces are
/// approximated — ICC profiles by their alternate or component count, Cal* as
/// device, overprint ignored — per the stage-4 plan.
struct ColorSpaceDef {
  ColorSpaceKind kind{ColorSpaceKind::unknown};
  /// Number of input components a colour in this space carries.
  int components{1};

  // Lab (8.6.5.4): the white point and the a*/b* component ranges.
  std::array<double, 3> white_point{0.9505, 1.0, 1.089};
  std::array<double, 4> lab_range{-100, 100, -100, 100};

  // Indexed (8.6.6.3): the base space, the packed palette and the max index.
  std::shared_ptr<ColorSpaceDef> base;
  std::string lookup;
  int hival{0};

  // Separation / DeviceN (8.6.6.4): the alternate space and the tint transform.
  std::shared_ptr<ColorSpaceDef> alternate;
  std::shared_ptr<Function> tint;

  /// Convert `components` of this space to sRGB in [0, 1]. A short/empty input
  /// yields the space's default colour.
  [[nodiscard]] std::array<double, 3>
  to_rgb(const std::vector<double> &components) const;

  /// The initial colour value of the space (ISO 32000-1 8.6.3): all-zero
  /// components, except Indexed (index 0) and Separation/DeviceN (tint 1.0).
  [[nodiscard]] std::vector<double> initial_components() const;
};

/// How `parse_color_space` reaches indirect data: `resolve` dereferences,
/// `load_stream` decodes a stream's bytes (ICC profiles, an Indexed palette
/// given as a stream), and `named` looks up a colour space referenced by name
/// (a base space, the `/ColorSpace` resource table).
struct ColorSpaceContext {
  std::function<Object(const Object &)> resolve;
  std::function<std::string(const Object &)> load_stream;
  std::function<std::shared_ptr<ColorSpaceDef>(const std::string &)> named;
};

/// Build a colour space from its PDF object — a name (`/DeviceRGB`, …) or an
/// array (`[/ICCBased 5 0 R]`, `[/Separation …]`, …). Returns `nullptr` for an
/// unsupported or malformed definition.
std::shared_ptr<ColorSpaceDef>
parse_color_space(const Object &object, const ColorSpaceContext &context);

} // namespace odr::internal::pdf
