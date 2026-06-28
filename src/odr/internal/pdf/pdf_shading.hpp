#pragma once

#include <odr/internal/pdf/pdf_color.hpp>
#include <odr/internal/pdf/pdf_function.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace odr::internal::pdf {

class Object;

/// One colour stop of a gradient: an `offset` in [0, 1] along the gradient axis
/// and the sRGB colour there. Sampled from a shading's `/Function` across its
/// `/Domain` at parse time, so the renderer needs no function evaluator.
struct GradientStop {
  double offset{0};
  std::array<double, 3> rgb{};
};

/// A resolved axial (type 2) or radial (type 3) shading (ISO 32000-1 8.7.4.5).
/// The tint `/Function` is pre-sampled into `stops`, so a renderer maps this to
/// an SVG `<linearGradient>`/`<radialGradient>` directly. Other shading types
/// (1, 4–7) are not modelled here (a later stage tessellates them).
///
/// NOTE: `extend`, `background` and `bbox` are parsed but not yet honoured by
/// the renderer — it always emits the gradient with SVG's `pad` spread, which
/// over-paints a non-extended shading beyond its interval (see
/// `GradientRegistry` in `html/pdf_file.cpp`). They are carried here so the
/// deferred bounds and background handling needs no re-parse.
struct Shading {
  /// `/ShadingType`: 2 (axial) or 3 (radial).
  std::int32_t type{0};
  /// Axial: `[x0 y0 x1 y1]` (the axis). Radial: `[x0 y0 r0 x1 y1 r1]` (the two
  /// circles). In shading space (mapped to user space by the caller's CTM or a
  /// pattern matrix).
  std::array<double, 6> coords{};
  /// `/Domain` `[t0 t1]` of the parametric variable (default `[0 1]`).
  std::array<double, 2> domain{0, 1};
  /// Colour stops sampled across `domain` (offsets in [0, 1], `stops.front()`
  /// at `t0`), in source order — at least two.
  std::vector<GradientStop> stops;
  /// `/Extend`: whether the shading continues beyond the axis ends. Parsed but
  /// not yet honoured (see the struct note).
  std::array<bool, 2> extend{false, false};
  /// `/Background` colour (sRGB), painted outside the shading where `/Extend`
  /// does not reach; absent when the shading declares none. Parsed but not yet
  /// honoured (see the struct note).
  bool has_background{false};
  std::array<double, 3> background{};
  /// `/BBox` `[x0 y0 x1 y1]` in shading space, clipping the shading; absent
  /// when none is declared. Parsed but not yet honoured (see the struct note).
  bool has_bbox{false};
  std::array<double, 4> bbox{};
};

/// How `parse_shading` reaches indirect data: `resolve` dereferences an object,
/// `load_stream` decodes a stream's bytes (a type-0 sampled `/Function`).
struct ShadingContext {
  std::function<Object(const Object &)> resolve;
  std::function<std::string(const Object &)> load_stream;
};

/// Build a shading from its `/Shading` dictionary, sampling its tint function
/// into `stops` through the shading's colour space (resolved via
/// `color_context`). Returns `nullptr` for a malformed or unsupported shading
/// (types other than 2/3).
std::shared_ptr<Shading> parse_shading(const Object &object,
                                       const ShadingContext &context,
                                       const ColorSpaceContext &color_context);

} // namespace odr::internal::pdf
