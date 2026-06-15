#include <odr/internal/pdf/pdf_page_text.hpp>

#include <odr/logger.hpp>

#include <odr/internal/pdf/pdf_document_element.hpp>
#include <odr/internal/pdf/pdf_graphics_operator.hpp>
#include <odr/internal/pdf/pdf_graphics_operator_parser.hpp>
#include <odr/internal/pdf/pdf_graphics_state.hpp>

#include <set>
#include <sstream>

namespace odr::internal::pdf {
namespace {

Font *lookup_font(const Resources &resources, const std::string &name,
                  const Logger &logger, std::set<std::string> &warned) {
  if (const auto it = resources.font.find(name); it != resources.font.end()) {
    return it->second;
  }
  if (warned.insert(name).second) {
    ODR_WARNING(logger, "pdf: unknown font resource '" + name +
                            "', emitting raw codes");
  }
  return nullptr;
}

/// Total advance of a shown string, in text-space units: the per-code glyph
/// width times the font size, plus char spacing (every code) and word spacing
/// (single-byte code 0x20 only), the whole scaled by horizontal scaling
/// (ISO 32000-1 9.4.4). 0 when the font is unknown.
double segment_advance(const GraphicsState::Text &text, const Font *font,
                       const std::string &codes) {
  if (font == nullptr) {
    return 0.0;
  }
  const int width = font->code_byte_width();
  double tx = 0.0;
  for (std::size_t i = 0; i + width <= codes.size(); i += width) {
    std::uint32_t code = 0;
    for (int k = 0; k < width; ++k) {
      code = (code << 8) | static_cast<unsigned char>(codes[i + k]);
    }
    tx += font->advance_width(code) * text.size + text.char_spacing;
    if (!font->composite && code == ' ') {
      tx += text.word_spacing;
    }
  }
  return tx * (text.horizontal_scaling / 100.0);
}

/// Emit one placed segment and advance the text matrix by its width.
void show(std::vector<TextElement> &out, GraphicsState &state,
          std::string codes, Font *font) {
  const GraphicsState::Text &text = state.current().text;
  const double advance = segment_advance(text, font, codes);

  TextElement element;
  element.transform = state.text_placement_transform();
  element.font = font;
  element.size = text.size;
  element.char_spacing = text.char_spacing;
  element.word_spacing = text.word_spacing;
  element.horizontal_scaling = text.horizontal_scaling;
  element.rise = text.rise;
  element.rendering_mode = text.rendering_mode;
  element.text = font != nullptr ? font->to_unicode(codes) : codes;
  element.codes = std::move(codes);
  element.width = advance;

  out.push_back(std::move(element));
  state.advance_text(advance, 0);
}

} // namespace

} // namespace odr::internal::pdf

namespace odr::internal {

std::vector<pdf::TextElement> pdf::extract_text(const std::string &content,
                                                const Resources &resources,
                                                const Logger &logger) {
  std::vector<TextElement> result;
  std::set<std::string> warned;

  std::istringstream ss(content);
  GraphicsOperatorParser parser(ss);
  GraphicsState state;

  while (!ss.eof()) {
    const GraphicsOperator op = parser.read_operator();
    state.execute(op);

    switch (op.type) {
    case GraphicsOperatorType::show_text:
    case GraphicsOperatorType::show_text_next_line: { // Tj, '
      Font *font =
          lookup_font(resources, state.current().text.font, logger, warned);
      show(result, state, op.arguments.at(0).as_string(), font);
    } break;
    case GraphicsOperatorType::show_text_manual_spacing: { // TJ
      Font *font =
          lookup_font(resources, state.current().text.font, logger, warned);
      const GraphicsState::Text &text = state.current().text;
      for (const Object &item : op.arguments.at(0).as_array()) {
        if (item.is_string()) {
          show(result, state, item.as_string(), font);
        } else if (item.is_real()) {
          // a number translates the next glyph left by adj/1000 text-space
          // units, scaled by the font size and horizontal scaling (9.4.3).
          const double adjust = -item.as_real() / 1000.0 * text.size *
                                (text.horizontal_scaling / 100.0);
          state.advance_text(adjust, 0);
        }
      }
    } break;
    case GraphicsOperatorType::show_text_next_line_set_spacing: { // "
      Font *font =
          lookup_font(resources, state.current().text.font, logger, warned);
      show(result, state, op.arguments.at(2).as_string(), font);
    } break;
    default:
      break;
    }
  }

  return result;
}

} // namespace odr::internal
