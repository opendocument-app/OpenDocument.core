#pragma once

#include <odr/internal/pdf/pdf_graphics_state.hpp>
#include <odr/internal/util/math_util.hpp>

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace odr::internal::pdf {

struct Font;
struct Shading;
struct XObject;
struct Pattern;
struct SoftMask;

/// One show-text operation laid out in user space. The font size is kept out of
/// `transform` so the renderer can choose per-run or per-glyph mapping.
struct TextElement {
  /// Text space -> user space, font size *not* applied (see
  /// `GraphicsState::text_placement_transform`).
  util::math::Transform2D transform;
  /// Null when the `/Font` resource name was unknown.
  Font *font{nullptr};
  double size{0};                                            // Tf size
  double char_spacing{0};                                    // Tc
  double word_spacing{0};                                    // Tw
  double horizontal_scaling{100};                            // Tz, percent
  double rise{0};                                            // Ts
  TextRenderingMode rendering_mode{TextRenderingMode::fill}; // Tr
  /// Device colours in force when the run was shown; the rendering mode selects
  /// which one paints (stroking only for `Tr` 1/5).
  GraphicsState::Color fill_color;
  GraphicsState::Color stroke_color;
  /// `/ExtGState` `ca`/`CA` and `/BM`; 1 = opaque, empty blend = Normal.
  double fill_alpha{1};
  double stroke_alpha{1};
  std::string blend_mode;
  /// Raw codes shown (one `Tj`, or one string of a `TJ` array).
  std::string codes;
  /// `codes` as Unicode, possibly with an inferred leading space. Empty when
  /// the segment carries no extractable text (see `no_unicode`, or an enclosing
  /// `/ActualText` already emitted the run).
  std::string text;
  /// The leading `U+0020` of `text` was inferred from a gap, so it backs no
  /// code and no advance. Consumers that pair codes with characters 1:1 (the
  /// HTML single-layer collapse test) must skip it.
  bool leading_space_inferred{false};
  /// The code -> Unicode chain yielded nothing, so `text` is empty and the run
  /// is displayable but not extractable. Cleared by an `/ActualText` override.
  bool no_unicode{false};
  /// Type3: the glyphs are emitted as separate path/image elements, so this
  /// element only carries selection/search — as for an invisible (`Tr` 3) run.
  bool render_as_graphics{false};
  /// Total advance in text-space units (font size, char/word spacing and
  /// horizontal scaling folded in), the sum of `advances`. 0 with no font.
  double width{0};
  /// Per-code advance, in code order. Empty with no font.
  std::vector<double> advances;
};

/// One path-painting operation (`S`/`s`/`f`/`F`/`f*`/`B`/…). The geometry is
/// fully resolved (the CTM applied at construction) and the paint-relevant
/// graphics state snapshotted, so a renderer needs no state replay.
struct PathElement {
  std::vector<Subpath> subpaths; ///< user space
  std::vector<ClipPath> clip;    ///< intersection; empty = unclipped
  bool fill{false};
  bool stroke{false};
  bool even_odd{false}; ///< fill rule: false = nonzero winding
  GraphicsState::Color fill_color;
  GraphicsState::Color stroke_color;
  /// A `/PatternType 2` fill (`scn`): paint this shading through the path
  /// instead of `fill_color`. `shading_transform` is the pattern `/Matrix`
  /// (shading space -> user space). Owned by `Resources`, which outlives us.
  const Shading *fill_shading{nullptr};
  util::math::Transform2D shading_transform;
  /// A `/PatternType 1` fill: tile this pattern's cell across the path. An
  /// uncoloured pattern (`/PaintType 2`) is painted in `fill_color`. Owned by
  /// `Resources`.
  const Pattern *fill_pattern{nullptr};
  util::math::Transform2D pattern_transform;
  /// Stroke parameters. `line_width` and the dash lengths carry the CTM scale,
  /// so they are in the geometry's user space; 0 means a device-thin line.
  double line_width{1};
  std::int32_t line_cap{0};
  std::int32_t line_join{0};
  double miter_limit{10};
  std::vector<double> dash_array; // empty = solid
  double dash_phase{0};
  /// `/ExtGState` `ca`/`CA` and `/BM`; 1 = opaque, empty blend = Normal.
  double fill_alpha{1};
  double stroke_alpha{1};
  std::string blend_mode;
  std::shared_ptr<const SoftMask> soft_mask; ///< `/ExtGState` `/SMask`
};

/// One image painted by `Do` or `BI`…`EI` (ISO 32000-1 8.10.5). The bytes are
/// browser-ready: a `DCTDecode` JPEG passed through, or a raster re-encoded as
/// PNG.
struct ImageElement {
  /// CTM at `Do` time: maps the image's unit square to user space.
  util::math::Transform2D transform;
  std::vector<ClipPath> clip;
  std::string data;
  std::string mime; // e.g. "image/jpeg"
  /// Null for an inline (`BI`) image and for a stencil.
  const XObject *source{nullptr};
  /// `/ExtGState` `ca` (the nonstroking alpha applies to images) and `/BM`.
  double alpha{1};
  std::string blend_mode;
  std::shared_ptr<const SoftMask> soft_mask;
};

/// One `sh` (ISO 32000-1 8.7.4.2): a shading flooded over the current clip,
/// with no path geometry of its own.
struct ShadingElement {
  const Shading *shading{nullptr};   ///< owned by `Resources`
  util::math::Transform2D transform; ///< shading space -> user space
  std::vector<ClipPath> clip;        ///< bounds the flood
  double alpha{1};                   ///< `/ExtGState` `ca`
  std::string blend_mode;
  std::shared_ptr<const SoftMask> soft_mask;
};

struct GroupChildren;

/// A transparency group painted as a unit (ISO 32000-1 11.6.6): composite the
/// content first, *then* apply the group-level alpha/blend/mask. Folding those
/// onto each interior element instead would show the overlaps through.
/// `children` is indirect to break the recursive-variant type cycle.
struct GroupElement {
  std::shared_ptr<const GroupChildren> children;
  double alpha{1}; ///< `ca`
  std::string blend_mode;
  std::shared_ptr<const SoftMask> soft_mask;
};

/// A single page-content element, in paint (z) order. Shading *patterns* ride
/// on `PathElement::fill_shading` rather than appearing here.
using PageElement = std::variant<TextElement, PathElement, ImageElement,
                                 ShadingElement, GroupElement>;

struct GroupChildren {
  std::vector<PageElement> elements;
};

/// A rendered soft mask (`/ExtGState` `/SMask`, ISO 32000-1 11.6.5.2): the
/// `/G` group already run through the page machinery into user space (its
/// `/Matrix` folded in, clipped to its `/BBox`). `type` says whether the
/// content's luminosity or its coverage is the alpha.
struct SoftMask {
  enum class Type { luminosity, alpha };
  Type type{Type::luminosity};
  std::vector<PageElement> group;
  std::optional<std::array<double, 3>> backdrop; ///< `/BC`; empty = black
};

} // namespace odr::internal::pdf
