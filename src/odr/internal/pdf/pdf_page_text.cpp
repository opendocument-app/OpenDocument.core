#include <odr/internal/pdf/pdf_page_text.hpp>

#include <odr/logger.hpp>

#include <odr/internal/pdf/pdf_document_element.hpp>
#include <odr/internal/pdf/pdf_graphics_operator.hpp>
#include <odr/internal/pdf/pdf_graphics_operator_parser.hpp>
#include <odr/internal/pdf/pdf_graphics_state.hpp>
#include <odr/internal/util/string_util.hpp>

#include <optional>
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

/// Decode a PDF text string (ISO 32000-1 7.9.2.2) to UTF-8: UTF-16BE when it
/// opens with the `FE FF` byte-order mark, otherwise PDFDocEncoding —
/// approximated here as Latin-1, which is exact over ASCII and the common
/// `/ActualText` payloads (the few upper-half divergences are rare).
std::string decode_text_string(const std::string &string) {
  if (string.size() >= 2 && static_cast<std::uint8_t>(string[0]) == 0xFE &&
      static_cast<std::uint8_t>(string[1]) == 0xFF) {
    std::u16string units;
    units.reserve((string.size() - 2) / 2);
    for (std::size_t i = 2; i + 1 < string.size(); i += 2) {
      units.push_back(
          static_cast<char16_t>((static_cast<std::uint8_t>(string[i]) << 8) |
                                static_cast<std::uint8_t>(string[i + 1])));
    }
    return util::string::u16string_to_string(units);
  }
  std::string result;
  for (const char c : string) {
    util::string::append_c32(static_cast<std::uint8_t>(c), result);
  }
  return result;
}

/// `/ActualText` of a `BDC` property-list dictionary, decoded to UTF-8, or
/// `nullopt` when the dictionary carries none.
std::optional<std::string> actual_text(const Dictionary &properties) {
  if (properties.has_key("ActualText") &&
      properties["ActualText"].is_string()) {
    return decode_text_string(properties["ActualText"].as_string());
  }
  return std::nullopt;
}

/// One open marked-content sequence (`BMC`/`BDC` … `EMC`). Only `/ActualText`
/// is modelled: when present it overrides the per-glyph Unicode of the whole
/// sequence (ISO 32000-1 14.9.4), emitted once (`consumed`) then suppressed.
struct MarkedContent {
  std::optional<std::string> actual_text;
  bool consumed{false};
};
using MarkedContentStack = std::vector<MarkedContent>;

/// Resolve the extraction text of a shown segment. The nearest enclosing
/// `/ActualText` sequence, if any, governs: its text is emitted for the first
/// segment and the rest of the sequence is suppressed (empty). Otherwise the
/// font's code -> Unicode chain applies; an empty result for non-empty codes is
/// reported as `no_unicode` (the run is not extractable until stage 3).
struct ResolvedText {
  std::string text;
  bool no_unicode{false};
};

ResolvedText resolve_text(MarkedContentStack &marked, const Font *font,
                          const std::string &codes) {
  for (auto it = marked.rbegin(); it != marked.rend(); ++it) {
    if (it->actual_text.has_value()) {
      if (it->consumed) {
        return {};
      }
      it->consumed = true;
      return {*it->actual_text, false};
    }
  }
  if (font == nullptr) {
    return {codes, false}; // unknown font: historic raw-code passthrough
  }
  std::string unicode = font->to_unicode(codes);
  const bool no_unicode = unicode.empty() && !codes.empty();
  return {std::move(unicode), no_unicode};
}

/// Per-code advances of a shown string and their total, in text-space units.
struct SegmentAdvances {
  /// Advance of each character code, in code order. Sums to `total`.
  std::vector<double> advances;
  /// Total advance applied to the text matrix after the segment.
  double total{0};
};

/// Advance of each code in a shown string and their total, in text-space units:
/// the per-code glyph width times the font size, plus char spacing (every code)
/// and word spacing (single-byte code 0x20 only), each scaled by horizontal
/// scaling (ISO 32000-1 9.4.4).
SegmentAdvances segment_advances(const GraphicsState::Text &text,
                                 const Font &font, const std::string &codes) {
  const int width = font.code_byte_width();
  const double scaling = text.horizontal_scaling / 100.0;
  SegmentAdvances result;
  for (std::size_t i = 0; i + width <= codes.size(); i += width) {
    std::uint32_t code = 0;
    for (int k = 0; k < width; ++k) {
      code = (code << 8) | static_cast<unsigned char>(codes[i + k]);
    }
    double tx = font.advance_width(code) * text.size + text.char_spacing;
    if (!font.composite && code == ' ') {
      tx += text.word_spacing;
    }
    tx *= scaling;
    result.advances.push_back(tx);
    result.total += tx;
  }
  return result;
}

/// Emit one placed segment and advance the text matrix by its width.
void show(std::vector<TextElement> &out, GraphicsState &state,
          MarkedContentStack &marked, std::string codes, Font *font) {
  const GraphicsState::Text &text = state.current().text;

  TextElement element;
  element.transform = state.text_placement_transform();
  element.font = font;
  element.size = text.size;
  element.char_spacing = text.char_spacing;
  element.word_spacing = text.word_spacing;
  element.horizontal_scaling = text.horizontal_scaling;
  element.rise = text.rise;
  element.rendering_mode = text.rendering_mode;
  ResolvedText resolved = resolve_text(marked, font, codes);
  element.text = std::move(resolved.text);
  element.no_unicode = resolved.no_unicode;
  element.codes = std::move(codes);
  if (font != nullptr) {
    auto [advances, total] = segment_advances(text, *font, element.codes);
    element.width = total;
    element.advances = std::move(advances);
  }

  const double advance = element.width;
  out.push_back(std::move(element));
  state.advance_text(advance, 0);
}

/// Form XObjects currently being rendered, by element identity. The parser
/// represents the file faithfully, so the XObject graph may contain cycles
/// (the spec forbids them — ISO 32000-1 8.10.1 — but real files err); this set
/// breaks them at render time.
using ActiveForms = std::set<const XObject *>;

void run_content(const std::string &content, const Resources &resources,
                 GraphicsState &state, std::vector<TextElement> &out,
                 const Logger &logger, std::set<std::string> &warned,
                 ActiveForms &active, MarkedContentStack &marked);

/// Open a marked-content sequence (`BMC`/`BDC`), recording its `/ActualText`
/// when one is present. `BDC`'s property list is either an inline dictionary or
/// a name into the `/Properties` resource subdictionary (ISO 32000-1 14.6.1).
void begin_marked_content(const GraphicsOperator &op,
                          const Resources &resources,
                          MarkedContentStack &marked) {
  MarkedContent entry;
  if (op.type == GraphicsOperatorType::begin_marked_content_seq_props &&
      op.arguments.size() >= 2) {
    const Object &props = op.arguments.at(1);
    if (props.is_dictionary()) {
      entry.actual_text = actual_text(props.as_dictionary());
    } else if (props.is_name()) {
      if (const auto it = resources.properties.find(props.as_name());
          it != resources.properties.end() && it->second.is_dictionary()) {
        entry.actual_text = actual_text(it->second.as_dictionary());
      }
    }
  }
  marked.push_back(std::move(entry));
}

/// Invoke a form XObject named by `Do`: save the state, concatenate the form's
/// `/Matrix` onto the CTM, run its content with the form's own `/Resources`
/// (falling back to the enclosing scope), then restore (ISO 32000-1 8.10.1).
/// `/BBox` clipping is deferred (text-only). Image and unknown XObjects are
/// skipped, and a form already on the render stack is skipped (cycle guard).
void invoke_x_object(const std::string &name, const Resources &resources,
                     GraphicsState &state, std::vector<TextElement> &out,
                     const Logger &logger, std::set<std::string> &warned,
                     ActiveForms &active, MarkedContentStack &marked) {
  const auto it = resources.x_object.find(name);
  if (it == resources.x_object.end()) {
    if (warned.insert("xobject:" + name).second) {
      ODR_WARNING(logger, "pdf: unknown XObject resource '" + name + "'");
    }
    return;
  }

  const XObject *x_object = it->second;
  if (x_object->subtype != XObject::Subtype::form) {
    return; // image XObjects are stage 4; unknown subtypes are inexecutable
  }
  if (!active.insert(x_object).second) {
    ODR_WARNING(logger, "pdf: cyclic form XObject invocation, skipping");
    return; // already on the render stack
  }

  state.save();
  state.concat_matrix(x_object->matrix);
  const Resources &scope =
      x_object->resources != nullptr ? *x_object->resources : resources;
  // A form's marked content must be self-balanced; truncate back to the entry
  // depth afterwards so an unbalanced form cannot corrupt the enclosing scope.
  const std::size_t depth = marked.size();
  run_content(x_object->content, scope, state, out, logger, warned, active,
              marked);
  marked.resize(depth);
  state.restore();
  active.erase(x_object);
}

void run_content(const std::string &content, const Resources &resources,
                 GraphicsState &state, std::vector<TextElement> &out,
                 const Logger &logger, std::set<std::string> &warned,
                 ActiveForms &active, MarkedContentStack &marked) {
  std::istringstream ss(content);
  GraphicsOperatorParser parser(ss);

  while (!ss.eof()) {
    const GraphicsOperator op = parser.read_operator();
    state.execute(op);

    switch (op.type) {
    case GraphicsOperatorType::show_text:
    case GraphicsOperatorType::show_text_next_line: { // Tj, '
      Font *font =
          lookup_font(resources, state.current().text.font, logger, warned);
      show(out, state, marked, op.arguments.at(0).as_string(), font);
    } break;
    case GraphicsOperatorType::show_text_manual_spacing: { // TJ
      Font *font =
          lookup_font(resources, state.current().text.font, logger, warned);
      const GraphicsState::Text &text = state.current().text;
      for (const Object &item : op.arguments.at(0).as_array()) {
        if (item.is_string()) {
          show(out, state, marked, item.as_string(), font);
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
      show(out, state, marked, op.arguments.at(2).as_string(), font);
    } break;
    case GraphicsOperatorType::draw_object: // Do
      invoke_x_object(op.arguments.at(0).as_string(), resources, state, out,
                      logger, warned, active, marked);
      break;
    case GraphicsOperatorType::begin_marked_content_seq:       // BMC
    case GraphicsOperatorType::begin_marked_content_seq_props: // BDC
      begin_marked_content(op, resources, marked);
      break;
    case GraphicsOperatorType::end_marked_content_seq: // EMC
      if (!marked.empty()) {
        marked.pop_back();
      }
      break;
    default:
      break;
    }
  }
}

} // namespace

} // namespace odr::internal::pdf

namespace odr::internal {

std::vector<pdf::TextElement> pdf::extract_text(const std::string &content,
                                                const Resources &resources,
                                                const Logger &logger) {
  std::vector<TextElement> result;
  std::set<std::string> warned;
  ActiveForms active;
  MarkedContentStack marked;
  GraphicsState state;

  run_content(content, resources, state, result, logger, warned, active,
              marked);

  return result;
}

} // namespace odr::internal
