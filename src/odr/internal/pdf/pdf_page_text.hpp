#pragma once

#include <odr/internal/pdf/pdf_geometry.hpp>

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
  Matrix transform;
  /// Resolved font, or `nullptr` when the `/Font` resource name was unknown.
  Font *font{nullptr};
  double size{0};                 // Tf size
  double char_spacing{0};         // Tc
  double word_spacing{0};         // Tw
  double horizontal_scaling{100}; // Tz, percent
  double rise{0};                 // Ts
  int rendering_mode{0};          // Tr
  /// Raw character codes shown (for `TJ`, the array's strings concatenated).
  std::string codes;
  /// Unicode representation of `codes`; may lack spaces the producer cannot
  /// infer (space inference is stage 2.5).
  std::string text;
};

/// Execute a page's (decoded, concatenated) content stream and collect the text
/// it shows as placed elements. Non-text operators update the graphics state
/// but produce no output. Glyph advances are not yet applied (stage 2.2), so
/// each show operation yields a single element at its starting origin.
std::vector<TextElement> extract_text(const std::string &content,
                                      const Resources &resources,
                                      const Logger &logger);

} // namespace odr::internal::pdf
