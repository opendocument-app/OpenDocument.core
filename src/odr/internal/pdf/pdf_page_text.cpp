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

void emit(std::vector<TextElement> &out, const GraphicsState &state,
          std::string codes, Font *font) {
  const GraphicsState::Text &text = state.current().text;

  TextElement element;
  element.transform = state.text_placement_matrix();
  element.font = font;
  element.size = text.size;
  element.char_spacing = text.char_spacing;
  element.word_spacing = text.word_spacing;
  element.horizontal_scaling = text.horizontal_scaling;
  element.rise = text.rise;
  element.rendering_mode = text.rendering_mode;
  element.text = font != nullptr ? font->to_unicode(codes) : codes;
  element.codes = std::move(codes);

  out.push_back(std::move(element));
}

/// Concatenate the string elements of a `TJ` array, dropping the numeric
/// position adjustments (their application is stage 2.2).
std::string join_array_strings(const Array &array) {
  std::string codes;
  for (const Object &element : array) {
    if (element.is_string()) {
      codes += element.as_string();
    }
  }
  return codes;
}

} // namespace

std::vector<TextElement> extract_text(const std::string &content,
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
      emit(result, state, op.arguments.at(0).as_string(), font);
    } break;
    case GraphicsOperatorType::show_text_manual_spacing: { // TJ
      Font *font =
          lookup_font(resources, state.current().text.font, logger, warned);
      emit(result, state, join_array_strings(op.arguments.at(0).as_array()),
           font);
    } break;
    case GraphicsOperatorType::show_text_next_line_set_spacing: { // "
      Font *font =
          lookup_font(resources, state.current().text.font, logger, warned);
      emit(result, state, op.arguments.at(2).as_string(), font);
    } break;
    default:
      break;
    }
  }

  return result;
}

} // namespace odr::internal::pdf
