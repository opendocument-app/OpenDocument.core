#pragma once

#include <odr/internal/pdf/pdf_graphics_state.hpp>
#include <odr/internal/util/math_util.hpp>

#include <string>
#include <variant>
#include <vector>

namespace odr::internal::pdf {

struct Font;
struct Shading;

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
  /// True when the font's code -> Unicode chain yielded nothing for this
  /// segment (a composite font with no `/ToUnicode` or usable predefined
  /// encoding is the common case), so `text` is empty. When the font has an
  /// embedded font its glyphs still display (re-encoded to the Private Use
  /// Area) with the run marked non-extractable; otherwise the run is simply not
  /// selectable. An `/ActualText` override clears this.
  bool no_unicode{false};
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
  /// Stroke parameters. `line_width` and the dash lengths are in the path's
  /// user space (the CTM scale is already folded in, so they live in the same
  /// space as the geometry). A `line_width` of 0 means a device-thin line.
  double line_width{1};
  int line_cap{0};
  int line_join{0};
  double miter_limit{10};
  std::vector<double> dash_array; // empty = solid
  double dash_phase{0};
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
};

/// A single page-content element in paint (z) order: a shown text segment, a
/// painted path, an image, or a shading flood (`sh`). Shading *patterns* ride
/// on `PathElement::fill_shading`; tiling patterns join in a later stage.
using PageElement =
    std::variant<TextElement, PathElement, ImageElement, ShadingElement>;

} // namespace odr::internal::pdf
