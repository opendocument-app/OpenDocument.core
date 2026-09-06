#pragma once

#include <odr/internal/pdf/pdf_object.hpp>

#include <array>
#include <string>
#include <vector>

namespace odr::internal::pdf {

class IncrementalWriter;
struct Page;

/// A text markup quadrilateral in user space: upper-left, upper-right,
/// lower-left, lower-right — the order producers write, not the
/// counterclockwise one 12.5.6.10 states.
using Quad = std::array<double, 8>;

/// One pen-down to pen-up stroke as flat `x y` pairs in user space.
using InkStroke = std::vector<double>;

enum class TextMarkupKind {
  highlight,  ///< 12.5.6.10, multiplied over the text it covers
  underline,  ///< a bar along the bottom of each quad
  strike_out, ///< a bar across the middle of each quad
  squiggly,   ///< a wave along the bottom of each quad
};

/// Fields every markup annotation carries (12.5.2).
struct AnnotationCommon {
  std::array<double, 3> color{0, 0, 0}; ///< `/C`, DeviceRGB in [0, 1]
  double opacity{1};                    ///< `/CA`
  std::string author;                   ///< `/T`, omitted when empty
  std::string contents;                 ///< `/Contents`, omitted when empty
};

struct TextMarkup {
  TextMarkupKind kind{TextMarkupKind::highlight};
  std::vector<Quad> quads;
  AnnotationCommon common;
};

struct Ink {
  std::vector<InkStroke> strokes;
  double width{1}; ///< `/BS /W`, in points
  AnnotationCommon common;
};

/// Write the annotation together with the appearance stream it paints through,
/// so no viewer has to synthesize one.
/// @throws std::invalid_argument on empty or malformed geometry.
ObjectReference write_text_markup(IncrementalWriter &writer,
                                  const TextMarkup &markup);
ObjectReference write_ink(IncrementalWriter &writer, const Ink &ink);

/// Append `annotations` to `page`'s `/Annots`, rewriting whichever object holds
/// it — the page dictionary, or the array itself where `/Annots` is indirect.
/// Reads the page as the source file has it, so call it once per page with
/// everything that page gains.
void append_page_annotations(IncrementalWriter &writer, const Page &page,
                             const std::vector<ObjectReference> &annotations);

} // namespace odr::internal::pdf
