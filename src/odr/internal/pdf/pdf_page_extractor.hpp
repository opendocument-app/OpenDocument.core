#pragma once

#include <odr/internal/pdf/pdf_page_element.hpp>

#include <vector>

namespace odr {
class Logger;
}

namespace odr::internal::pdf {

struct Annotation;
struct Resources;

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

/// Execute a page's (decoded, concatenated) content stream and collect every
/// painted element — text segments and paths — as placed `PageElement`s in
/// paint (z) order, so a renderer can interleave them faithfully. Text segments
/// are exactly those `extract_text` returns; path elements carry the
/// constructed geometry and the paint state. `extract_text` is the text-only
/// projection of this list.
std::vector<PageElement> extract_page(const std::string &content,
                                      const Resources &resources,
                                      const Logger &logger);

/// Execute an annotation's normal appearance stream (ISO 32000-1 12.5.5),
/// returning what it paints in the page's own space. `/CA` under 1 wraps the
/// result in a group. Empty when there is no appearance to paint.
std::vector<PageElement> extract_annotation(const Annotation &annotation,
                                            const Logger &logger);

} // namespace odr::internal::pdf
