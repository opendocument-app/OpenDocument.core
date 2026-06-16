#pragma once

#include <odr/internal/util/math_util.hpp>

#include <string>
#include <vector>

namespace odr {
class Logger;
}

namespace odr::internal::pdf {

struct Resources;
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
  /// Unicode representation of `codes`; may lack spaces the producer cannot
  /// infer (space inference is stage 2.5).
  std::string text;
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

/// Execute a page's (decoded, concatenated) content stream and collect the text
/// it shows as placed elements. Non-text operators update the graphics state
/// but produce no output. Each shown segment (one `Tj`/`'`/`"`, or one string
/// of a `TJ` array) yields one element at its origin; the text matrix is
/// advanced by the glyph widths (`font->advance_width`) plus char/word spacing
/// and the `TJ` numeric adjustments, so segments and lines land in the right
/// place.
std::vector<TextElement> extract_text(const std::string &content,
                                      const Resources &resources,
                                      const Logger &logger);

} // namespace odr::internal::pdf
