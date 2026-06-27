#pragma once

#include <odr/internal/pdf/pdf_graphics_state.hpp>
#include <odr/internal/util/math_util.hpp>

#include <string>
#include <variant>
#include <vector>

namespace odr::internal::pdf {

struct Font;

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
  double size{0};                 // Tf size
  double char_spacing{0};         // Tc
  double word_spacing{0};         // Tw
  double horizontal_scaling{100}; // Tz, percent
  double rise{0};                 // Ts
  int rendering_mode{0};          // Tr
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
/// device colors and converted to RGB by the renderer. `/Pattern` color is
/// stage 4.9+ and not yet represented.
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
/// which `transform` maps. The encoded bytes pass straight through to the
/// browser (stage 4.5: JPEG / `DCTDecode`), `mime` naming the codec. The clip
/// is snapshotted as for a path.
struct ImageElement {
  /// CTM at `Do` time: maps the image's unit square to user space.
  util::math::Transform2D transform;
  std::vector<ClipPath> clip;
  std::string data; // encoded image bytes (e.g. a JPEG)
  std::string mime; // e.g. "image/jpeg"
};

/// A single page-content element in paint (z) order: a shown text segment, a
/// painted path or an image. Shadings and patterns join this variant in later
/// stage-4 PRs.
using PageElement = std::variant<TextElement, PathElement, ImageElement>;

} // namespace odr::internal::pdf
