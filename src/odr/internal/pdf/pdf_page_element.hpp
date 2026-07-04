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
struct Pattern;
struct SoftMask;

/// One show-text operation laid out in user space. The transform places the
/// text origin and orientation; the font size is kept separate so the renderer
/// can choose per-run or per-glyph mapping. Spacing parameters and the raw
/// codes are carried for the (deferred) advance application.
struct TextElement {
  /// Text-space -> user-space, font size *not* applied (see
  /// `GraphicsState::text_placement_matrix`).
  util::math::Transform2D transform;
  /// Resolved font, or `nullptr` when the `/Font` resource name was unknown.
  Font *font{nullptr};
  double size{0};                                            // Tf size
  double char_spacing{0};                                    // Tc
  double word_spacing{0};                                    // Tw
  double horizontal_scaling{100};                            // Tz, percent
  double rise{0};                                            // Ts
  TextRenderingMode rendering_mode{TextRenderingMode::fill}; // Tr
  /// Non-stroking (fill) and stroking colours in force when the run was shown,
  /// as device colours. The renderer paints the run in whichever its rendering
  /// mode selects — the fill colour for the common fill modes, the stroking
  /// colour for the stroke-only modes (Tr 1/5) — defaulting to black.
  GraphicsState::Color fill_color;
  GraphicsState::Color stroke_color;
  /// Constant fill/stroke alpha (`/ExtGState` `ca`/`CA`) and blend mode
  /// (`/BM`) in force when the run was shown; 1 = opaque, empty blend = Normal.
  double fill_alpha{1};
  double stroke_alpha{1};
  std::string blend_mode;
  /// Raw character codes shown by this segment (one `Tj`, or one string of a
  /// `TJ` array).
  std::string codes;
  /// Unicode representation of `codes`, with an inferred leading space when a
  /// large enough gap precedes this segment (space inference) so the
  /// producer's omitted inter-word/-line spaces are recovered. Empty when the
  /// segment carries no extractable text — either the code -> Unicode chain
  /// yielded nothing (see `no_unicode`) or an enclosing `/ActualText` already
  /// emitted the run's text.
  std::string text;
  /// True when the leading character of `text` is an inferred inter-word/-line
  /// space (space inference), i.e. one prepended `U+0020` with no backing
  /// character code or advance. Lets a consumer treat that space as metadata
  /// (selectable but not part of the 1:1 code<->char run) rather than content:
  /// the single-layer HTML collapse test skips it, so a run that would map
  /// cleanly to real Unicode is not forced onto the PUA glyph path merely
  /// because a word-break space was recovered in front of it.
  bool leading_space_inferred{false};
  /// True when the font's code -> Unicode chain yielded nothing for this
  /// segment (a composite font with no `/ToUnicode` or usable predefined
  /// encoding is the common case), so `text` is empty. When the font has an
  /// embedded font its glyphs still display (re-encoded to the Private Use
  /// Area) with the run marked non-extractable; otherwise the run is simply not
  /// selectable. An `/ActualText` override clears this.
  bool no_unicode{false};
  /// True for a Type3 font: the glyphs are drawn as ordinary path/image
  /// elements (the char procs, emitted alongside this element in paint order),
  /// so the renderer keeps this element for selection/search but paints no
  /// visible text of its own — as it does for an invisible (Tr 3) run.
  bool render_as_graphics{false};
  /// Total advance of this segment, in text-space units (the displacement
  /// applied to the text matrix after it — already scaled by the font size and
  /// including char/word spacing and horizontal scaling). 0 when the font is
  /// unknown. Equal to the sum of `advances`.
  double width{0};
  /// Per-character-code advance, in code order and text-space units, summing to
  /// `width`. Empty when the font is unknown. Lets a renderer place glyphs
  /// individually without re-deriving widths from `font->advance_width`.
  std::vector<double> advances;
};

/// One path-painting operation (`S`/`s`/`f`/`F`/`f*`/`B`/…), laid out in user
/// space. The geometry is fully resolved (the CTM applied at construction), so
/// a renderer maps it through the page transform alone. The paint intent and
/// the paint-relevant graphics state are snapshotted; colors are kept as PDF
/// device colors and converted to RGB by the renderer. A `/PatternType 2`
/// shading pattern fill is carried by `fill_shading`; tiling patterns
/// (`/PatternType 1`) are a later stage.
struct PathElement {
  /// The subpaths to paint, in user space.
  std::vector<Subpath> subpaths;
  /// The clip in force when this path was painted: the intersection of these
  /// regions (empty = unclipped). Snapshotted from the graphics state at paint
  /// time so the renderer can install it without replaying the clip stack.
  std::vector<ClipPath> clip;
  bool fill{false};
  bool stroke{false};
  /// Fill rule: false = nonzero winding, true = even-odd.
  bool even_odd{false};
  /// Non-stroking (fill) color and stroking color, as device colors.
  GraphicsState::Color fill_color;
  GraphicsState::Color stroke_color;
  /// When the fill is a shading pattern (`scn` naming a `/PatternType 2`
  /// pattern), the resolved shading to paint through the path instead of
  /// `fill_color`, with `shading_transform` mapping shading space to user space
  /// (the pattern `/Matrix`). Null for a plain colour fill. Owned by
  /// `Resources`, which outlives the element.
  const Shading *fill_shading{nullptr};
  util::math::Transform2D shading_transform;
  /// When the fill is a tiling pattern (`scn` naming a `/PatternType 1`
  /// pattern), the resolved pattern whose content cell tiles the path, with
  /// `pattern_transform` mapping pattern space to user space (the pattern
  /// `/Matrix`). An uncoloured pattern (`/PaintType 2`) is painted in
  /// `fill_color`. Null for a non-tiling fill. Owned by `Resources`.
  const Pattern *fill_pattern{nullptr};
  util::math::Transform2D pattern_transform;
  /// Stroke parameters. `line_width` and the dash lengths are in the path's
  /// user space (the CTM scale is already folded in, so they live in the same
  /// space as the geometry). A `line_width` of 0 means a device-thin line.
  double line_width{1};
  std::int32_t line_cap{0};
  std::int32_t line_join{0};
  double miter_limit{10};
  std::vector<double> dash_array; // empty = solid
  double dash_phase{0};
  /// Constant fill/stroke alpha (`/ExtGState` `ca`/`CA`) and blend mode (`/BM`)
  /// in force when the path was painted; 1 = opaque, empty blend = Normal.
  double fill_alpha{1};
  double stroke_alpha{1};
  std::string blend_mode;
  /// Soft mask (`/ExtGState` `/SMask`) in force when the path was painted, or
  /// null when none. Snapshotted like the clip so the renderer can install it
  /// without replaying the graphics state.
  std::shared_ptr<const SoftMask> soft_mask;
};

/// One image XObject painted by `Do`, placed by the CTM in effect when it was
/// invoked (ISO 32000-1 8.10.5): the image fills the unit square in user space,
/// which `transform` maps. The encoded bytes are browser-ready — a JPEG passed
/// through (`DCTDecode`) or a raster re-encoded as PNG — with `mime` naming the
/// format. The clip is snapshotted as for a path.
struct ImageElement {
  /// CTM at `Do` time: maps the image's unit square to user space.
  util::math::Transform2D transform;
  std::vector<ClipPath> clip;
  std::string data; // encoded image bytes (e.g. a JPEG)
  std::string mime; // e.g. "image/jpeg"
  /// Constant alpha (`/ExtGState` `ca`, the nonstroking alpha applies to
  /// images) and blend mode (`/BM`) in force at `Do`; 1 = opaque, empty blend =
  /// Normal.
  double alpha{1};
  std::string blend_mode;
  /// Soft mask (`/ExtGState` `/SMask`) in force at `Do`, or null when none.
  std::shared_ptr<const SoftMask> soft_mask;
};

/// One area painted by the `sh` operator (ISO 32000-1 8.7.4.2): a shading
/// flooded over the current clip region (no path geometry of its own). The
/// transform maps shading space to user space (the CTM at `sh` time); the clip
/// is snapshotted as for a path. The renderer paints the shading's gradient
/// across the clipped area.
struct ShadingElement {
  /// The shading to paint. Owned by `Resources`, which outlives the element.
  const Shading *shading{nullptr};
  /// Shading space -> user space (the CTM in force at `sh` time).
  util::math::Transform2D transform;
  /// The clip in force, snapshotted so the renderer bounds the flood.
  std::vector<ClipPath> clip;
  /// Constant fill alpha (`/ExtGState` `ca`) and blend mode (`/BM`) in force at
  /// `sh`; 1 = opaque, empty blend = Normal.
  double alpha{1};
  std::string blend_mode;
  /// Soft mask (`/ExtGState` `/SMask`) in force at `sh`, or null when none.
  std::shared_ptr<const SoftMask> soft_mask;
};

struct GroupChildren;

/// A transparency group painted as a unit (ISO 32000-1 11.6.6): its content is
/// composited first, then the whole result is painted with the group-level
/// constant alpha, blend mode and soft mask in force when the group was
/// invoked. This is *not* the same as folding those onto each interior element
/// — for content whose paints overlap (a figure's hair over its face, say) the
/// group must composite opaquely first, then fade, or the overlaps show
/// through. The renderer emits the children into one container (`<g>`) carrying
/// the group parameters. `children` is held indirectly so `PageElement` can
/// hold a `GroupElement` (a recursive variant).
struct GroupElement {
  std::shared_ptr<const GroupChildren> children;
  /// Group-level nonstroking constant alpha (`ca`), 1 = opaque.
  double alpha{1};
  /// Group-level blend mode (`/BM`); empty = Normal.
  std::string blend_mode;
  /// Group-level soft mask (`/SMask`), or null when none.
  std::shared_ptr<const SoftMask> soft_mask;
};

/// A single page-content element in paint (z) order: a shown text segment, a
/// painted path, an image, a shading flood (`sh`), or a transparency group.
/// Shading *patterns* ride on `PathElement::fill_shading`; tiling patterns join
/// in a later stage.
using PageElement = std::variant<TextElement, PathElement, ImageElement,
                                 ShadingElement, GroupElement>;

/// The content of a `GroupElement`, in paint order. A distinct type (rather
/// than a bare `std::vector<PageElement>` member) breaks the `PageElement` ->
/// `GroupElement` -> `PageElement` type cycle.
struct GroupChildren {
  std::vector<PageElement> elements;
};

/// A rendered soft mask (`/SMask` in an `/ExtGState`, ISO 32000-1 11.6.5.2),
/// ready for a renderer to apply to the elements that reference it. The mask's
/// transparency group `/G` has been run through the page machinery into `group`
/// (its content in user space, the establishment CTM and the group `/Matrix`
/// already folded in and clipped to the group `/BBox`). The alpha is derived
/// from that content per `type`: `luminosity` (the content's luminosity is the
/// alpha — an SVG luminance `<mask>`) or `alpha` (its coverage is). `backdrop`
/// is the `/BC` colour as RGB behind the group (default black, i.e. fully
/// masked out where the group paints nothing); empty when the default applies.
struct SoftMask {
  enum class Type { luminosity, alpha };
  Type type{Type::luminosity};
  std::vector<PageElement> group;
  std::optional<std::array<double, 3>> backdrop; // /BC as RGB; empty = black
};

} // namespace odr::internal::pdf
