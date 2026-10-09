#pragma once

#include <odr/internal/pdf/pdf_page_element.hpp>

#include <vector>

namespace odr {
class Logger;
}

namespace odr::internal::pdf {

struct Annotation;
struct Resources;

/// Extracts placed text from decoded content, including glyph advances,
/// character/word spacing and `TJ` adjustments; excludes graphics.
std::vector<TextElement> extract_text(const std::string &content,
                                      const Resources &resources,
                                      const Logger &logger);

/// Executes decoded content into placed text and graphics in paint order.
/// `extract_text` returns the text-only projection.
std::vector<PageElement> extract_page(const std::string &content,
                                      const Resources &resources,
                                      const Logger &logger);

/// Execute an annotation's normal appearance stream (ISO 32000-1 12.5.5),
/// returning what it paints in the page's own space. `/CA` under 1 wraps the
/// result in a group. Empty when there is no appearance to paint.
std::vector<PageElement> extract_annotation(const Annotation &annotation,
                                            const Logger &logger);

} // namespace odr::internal::pdf
