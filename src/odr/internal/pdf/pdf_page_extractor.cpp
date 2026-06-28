#include <odr/internal/pdf/pdf_page_extractor.hpp>

#include <odr/logger.hpp>

#include <odr/internal/pdf/pdf_color.hpp>
#include <odr/internal/pdf/pdf_document_element.hpp>
#include <odr/internal/pdf/pdf_filter.hpp>
#include <odr/internal/pdf/pdf_graphics_operator.hpp>
#include <odr/internal/pdf/pdf_graphics_operator_parser.hpp>
#include <odr/internal/pdf/pdf_graphics_state.hpp>
#include <odr/internal/pdf/pdf_image.hpp>
#include <odr/internal/util/string_util.hpp>

#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <unordered_map>

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

/// Map a single PDFDocEncoding byte to its Unicode code point (ISO 32000-1
/// Annex D.2). It agrees with Latin-1 over ASCII and 0xA1–0xFF, so only the
/// diacritic block (0x18–0x1F), the typographic block (0x80–0x9E), and the
/// euro (0xA0) need overriding; every other byte stands for the code point of
/// the same value. The few undefined slots (0x7F, 0x9F, 0xAD) pass through.
char32_t pdf_doc_encoding_to_unicode(const std::uint8_t byte) {
  switch (byte) {
  case 0x18:
    return U'˘'; // breve
  case 0x19:
    return U'ˇ'; // caron
  case 0x1A:
    return U'ˆ'; // circumflex
  case 0x1B:
    return U'˙'; // dot above
  case 0x1C:
    return U'˝'; // double acute
  case 0x1D:
    return U'˛'; // ogonek
  case 0x1E:
    return U'˚'; // ring above
  case 0x1F:
    return U'˜'; // small tilde
  case 0x80:
    return U'•'; // bullet
  case 0x81:
    return U'†'; // dagger
  case 0x82:
    return U'‡'; // double dagger
  case 0x83:
    return U'…'; // ellipsis
  case 0x84:
    return U'—'; // em dash
  case 0x85:
    return U'–'; // en dash
  case 0x86:
    return U'ƒ'; // florin
  case 0x87:
    return U'⁄'; // fraction slash
  case 0x88:
    return U'‹'; // single left angle quote
  case 0x89:
    return U'›'; // single right angle quote
  case 0x8A:
    return U'−'; // minus
  case 0x8B:
    return U'‰'; // per mille
  case 0x8C:
    return U'„'; // double low-9 quote
  case 0x8D:
    return U'“'; // left double quote
  case 0x8E:
    return U'”'; // right double quote
  case 0x8F:
    return U'‘'; // left single quote
  case 0x90:
    return U'’'; // right single quote
  case 0x91:
    return U'‚'; // single low-9 quote
  case 0x92:
    return U'™'; // trademark
  case 0x93:
    return U'ﬁ'; // fi ligature
  case 0x94:
    return U'ﬂ'; // fl ligature
  case 0x95:
    return U'Ł'; // L with stroke
  case 0x96:
    return U'Œ'; // OE
  case 0x97:
    return U'Š'; // S with caron
  case 0x98:
    return U'Ÿ'; // Y with diaeresis
  case 0x99:
    return U'Ž'; // Z with caron
  case 0x9A:
    return U'ı'; // dotless i
  case 0x9B:
    return U'ł'; // l with stroke
  case 0x9C:
    return U'œ'; // oe
  case 0x9D:
    return U'š'; // s with caron
  case 0x9E:
    return U'ž'; // z with caron
  case 0xA0:
    return U'€'; // euro
  default:
    return byte;
  }
}

/// Decode a PDF text string (ISO 32000-1 7.9.2.2) to UTF-8: UTF-16BE when it
/// opens with the `FE FF` byte-order mark, otherwise PDFDocEncoding.
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
    util::string::append_c32(
        pdf_doc_encoding_to_unicode(static_cast<std::uint8_t>(c)), result);
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
/// reported as `no_unicode` (the run is not extractable, only displayable).
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
  const double scaling = text.horizontal_scaling / 100.0;
  SegmentAdvances result;
  for (const std::uint32_t code : font.codes(codes)) {
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

/// The pen left by the previous shown segment, in user space, used to infer
/// inter-word spaces PDFs routinely omit. `position` is the segment
/// origin *after* its advance; `direction`/`em` give the writing line and scale
/// the gap threshold; `trailing_space` suppresses a doubled space.
struct Pen {
  std::array<double, 2> position{0, 0};
  std::array<double, 2> direction{1, 0};
  double em{0};
  bool trailing_space{false};
};

/// Whether a space should be inferred before a segment starting at `start`
/// (user space). A forward gap past ~0.2 em along the writing line (a word
/// break) or a perpendicular jump past ~0.5 em (a new line) triggers one,
/// scaled by the previous segment's em so it tracks the font size.
bool infer_space(const Pen &pen, const std::array<double, 2> &start) {
  if (pen.em <= 0 || pen.trailing_space) {
    return false;
  }
  const double gap_x = start[0] - pen.position[0];
  const double gap_y = start[1] - pen.position[1];
  const double along = gap_x * pen.direction[0] + gap_y * pen.direction[1];
  const double perp = std::hypot(gap_x - along * pen.direction[0],
                                 gap_y - along * pen.direction[1]);
  constexpr double word_gap = 0.2;
  constexpr double new_line = 0.5;
  if (perp > new_line * pen.em) {
    return true; // a different line
  }
  return along > word_gap * pen.em; // a within-line word break
}

/// Emit one placed segment and advance the text matrix by its width.
void show(std::vector<PageElement> &out, GraphicsState &state,
          MarkedContentStack &marked, std::optional<Pen> &pen,
          std::string codes, Font *font) {
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
  element.fill_color = state.current().other_color;
  element.stroke_color = state.current().stroke_color;
  ResolvedText resolved = resolve_text(marked, font, codes);
  element.text = std::move(resolved.text);
  element.no_unicode = resolved.no_unicode;
  element.codes = std::move(codes);
  if (font != nullptr) {
    auto [advances, total] = segment_advances(text, *font, element.codes);
    element.width = total;
    element.advances = std::move(advances);
  }

  // Space inference: prepend a space when the segment starts far
  // enough past the previous one's pen. The geometry is read from the placement
  // transform (origin and writing-line basis) in user space.
  const util::math::Transform2D &m = element.transform;
  const std::array<double, 2> start{m.e, m.f};
  const double basis = std::hypot(m.a, m.b);
  const std::array<double, 2> direction =
      basis > 0 ? std::array<double, 2>{m.a / basis, m.b / basis}
                : std::array<double, 2>{1, 0};
  if (!element.text.empty() && element.text.front() != ' ' && pen.has_value() &&
      infer_space(*pen, start)) {
    element.text.insert(element.text.begin(), ' ');
  }
  // A segment with no extractable text (suppressed glyphs, `no_unicode`, or a
  // subsequent show inside an already-emitted `/ActualText` span) must not
  // clear a trailing space carried by the previous segment, or the next gap
  // would infer a second space.
  const bool trailing_space = element.text.empty()
                                  ? (pen.has_value() && pen->trailing_space)
                                  : element.text.back() == ' ';

  const double advance = element.width;
  out.push_back(std::move(element));
  state.advance_text(advance, 0);

  // Record the pen at the true post-advance origin (so `TJ` adjustments and any
  // explicit repositioning before the next segment fold into the next gap).
  const util::math::Transform2D after = state.text_placement_transform();
  pen = Pen{{after.e, after.f}, direction, text.size * basis, trailing_space};
}

/// Resolve a colour-space operator (`cs`/`CS`): set the active colour space on
/// `color` from the resource `/ColorSpace` table (or a device space named
/// inline) and reset it to the space's initial colour (ISO 32000-1 8.6.8).
void set_color_space(GraphicsState::Color &color, const std::string &name,
                     const Resources &resources) {
  color = GraphicsState::Color{};
  if (name == "DeviceGray" || name == "G") {
    color.space = ColorSpace::device_grey;
  } else if (name == "DeviceRGB" || name == "RGB") {
    color.space = ColorSpace::device_rgb;
  } else if (name == "DeviceCMYK" || name == "CMYK") {
    color.space = ColorSpace::device_cmyk;
    color.cmyk = {0, 0, 0, 1}; // initial DeviceCMYK colour is black
  } else if (const auto it = resources.color_space.find(name);
             it != resources.color_space.end() && it->second != nullptr) {
    color.def = it->second.get();
    color.space = ColorSpace::device_rgb;
    color.rgb = color.def->to_rgb(color.def->initial_components());
  } else {
    color.space = ColorSpace::unknown;
  }
}

/// Resolve a general colour operator (`sc`/`scn`/`SC`/`SCN`): convert the
/// operand components through the active colour space to RGB. With no resource
/// colour space, interpret the components as a device colour by their count
/// (ISO 32000-1 8.6.8). A trailing name operand selects a `/Pattern`: its name
/// is recorded on `color.pattern` and resolved against `Resources::pattern` at
/// paint time (a shading pattern then fills the path through its gradient).
void set_color(GraphicsState::Color &color, const GraphicsOperator &op) {
  std::vector<double> components;
  std::string pattern_name;
  for (const Object &argument : op.arguments) {
    if (argument.is_name()) {
      pattern_name = argument.as_name();
    } else if (argument.is_real()) {
      components.push_back(argument.as_real());
    }
  }
  color.pattern = pattern_name;
  if (!pattern_name.empty()) {
    // A pattern colour carries no device components to convert; the pattern is
    // resolved at paint time. Leave any underlying colour as-is.
    return;
  }
  if (color.def != nullptr) {
    color.space = ColorSpace::device_rgb;
    color.rgb = color.def->to_rgb(components);
    return;
  }
  switch (components.size()) {
  case 1:
    color.space = ColorSpace::device_grey;
    color.grey = components[0];
    break;
  case 3:
    color.space = ColorSpace::device_rgb;
    color.rgb = {components[0], components[1], components[2]};
    break;
  case 4:
    color.space = ColorSpace::device_cmyk;
    color.cmyk = {components[0], components[1], components[2], components[3]};
    break;
  default:
    break;
  }
}

/// Resolve a graphics-state colour to sRGB in [0, 1]. Non-device spaces have
/// already been converted to `rgb` by `set_color`/`set_color_space` (they set
/// `space` to `device_rgb`); CMYK uses the same naive conversion as the HTML
/// emitter. Used to paint a stencil image mask in the current fill colour.
std::array<double, 3> color_to_rgb(const GraphicsState::Color &color) {
  switch (color.space) {
  case ColorSpace::device_grey:
    return {color.grey, color.grey, color.grey};
  case ColorSpace::device_rgb:
    return {color.rgb[0], color.rgb[1], color.rgb[2]};
  case ColorSpace::device_cmyk: {
    const double c = color.cmyk[0];
    const double m = color.cmyk[1];
    const double y = color.cmyk[2];
    const double k = color.cmyk[3];
    return {(1 - c) * (1 - k), (1 - m) * (1 - k), (1 - y) * (1 - k)};
  }
  case ColorSpace::unknown:
    break;
  }
  return {0, 0, 0};
}

/// Emit a path-painting element from the path accumulated in `state` and the
/// current paint state, then clear the path (as every painting operator does).
/// `close` first closes the current subpath (the `s`/`b`/`b*` variants).
void paint_path(std::vector<PageElement> &out, const Resources &resources,
                GraphicsState &state, const bool fill, const bool stroke,
                const bool even_odd, const bool close) {
  if (close) {
    state.path_close();
  }
  const GraphicsState::State &s = state.current();
  PathElement element;
  element.subpaths = state.path;
  // The path is painted under the clip in force *before* a pending `W`/`W*` is
  // installed (ISO 32000-1 8.5.4): snapshot the current clip, then commit.
  element.clip = s.clip;
  element.fill = fill;
  element.stroke = stroke;
  element.even_odd = even_odd;
  element.fill_color = s.other_color;
  element.stroke_color = s.stroke_color;
  // A `/Pattern`-coloured fill: resolve the pattern selected by `scn`. A
  // shading pattern (`/PatternType 2`) paints its gradient through the path;
  // its matrix maps shading space to the page's default user space (ISO 32000-1
  // 8.7.3.1). Other pattern types fall through to the plain fill colour.
  if (fill && !s.other_color.pattern.empty()) {
    if (const auto it = resources.pattern.find(s.other_color.pattern);
        it != resources.pattern.end() && it->second != nullptr) {
      const Pattern *pattern = it->second;
      if (pattern->type == Pattern::Type::shading &&
          pattern->shading != nullptr) {
        element.fill_shading = pattern->shading.get();
        element.shading_transform = pattern->matrix;
      }
    }
  }
  // The stroke width and dash lengths are given in the CTM's space; fold the
  // CTM scale in so they live in the same user space as the geometry. Use the
  // area-preserving factor sqrt|det| (an anisotropic CTM can't be expressed as
  // a single SVG pen width; ISO 32000-1 8.4.3.2).
  const util::math::Transform2D &m = s.general.transform_matrix;
  const double scale = std::sqrt(std::abs(m.a * m.d - m.b * m.c));
  element.line_width = s.general.line_width * scale;
  element.line_cap = s.general.cap_style;
  element.line_join = s.general.join_style;
  element.miter_limit = s.general.miter_limit;
  element.dash_array.reserve(s.general.dash.array.size());
  for (const double on_off : s.general.dash.array) {
    element.dash_array.push_back(on_off * scale);
  }
  element.dash_phase = s.general.dash.phase * scale;
  // Construct the variant alternative in place: building a temporary variant
  // and move-constructing it (`push_back(std::move(element))`) trips a spurious
  // GCC 14 `-Wmaybe-uninitialized` on the string-bearing `ImageElement`
  // alternative while analyzing the union move, which `-Werror` makes fatal.
  out.emplace_back(std::in_place_type<PathElement>, std::move(element));
  // Install a pending `W`/`W*` now (it uses the just-painted path) so it scopes
  // the *following* content, then drop the path.
  state.commit_clip();
  state.clear_path();
}

/// Map an inline image's abbreviated dictionary key to its long form
/// (ISO 32000-1 Table 93), so the shared image path sees the same keys as an
/// image XObject. Unknown keys pass through unchanged.
const std::string &inline_image_long_key(const std::string &key) {
  static const std::unordered_map<std::string, std::string> map = {
      {"BPC", "BitsPerComponent"}, {"CS", "ColorSpace"}, {"D", "Decode"},
      {"DP", "DecodeParms"},       {"F", "Filter"},      {"H", "Height"},
      {"IM", "ImageMask"},         {"I", "Interpolate"}, {"W", "Width"}};
  const auto it = map.find(key);
  return it != map.end() ? it->second : key;
}

/// Rewrite an inline image dictionary's abbreviated keys to their long forms.
/// (Abbreviated *values* — the filter names and the `/CS` device names — are
/// understood by `pdf_filter` and `pdf_color` directly.)
Dictionary normalize_inline_image(const Dictionary &raw) {
  Dictionary result;
  for (const auto &[key, value] : raw) {
    result[inline_image_long_key(key)] = value;
  }
  return result;
}

/// Resolve an inline image's colour space. Inline content is self-contained (no
/// indirect references), so `resolve`/`load_stream` are inert; a bare name that
/// is not a device space is looked up in the page's `/ColorSpace` resources.
std::shared_ptr<ColorSpaceDef>
resolve_inline_color_space(const Object &color_space,
                           const Resources &resources) {
  if (color_space.is_null()) {
    return nullptr;
  }
  ColorSpaceContext context;
  context.resolve = [](const Object &o) { return o; };
  context.load_stream = [](const Object &) { return std::string{}; };
  context.named =
      [&resources](const std::string &name) -> std::shared_ptr<ColorSpaceDef> {
    const auto it = resources.color_space.find(name);
    return it != resources.color_space.end() ? it->second : nullptr;
  };
  return parse_color_space(color_space, context);
}

/// Emit an `ImageElement` for an inline image (`BI`/`ID`/`EI`). The operator
/// carries the inline dictionary and the raw bytes; both are routed through the
/// same image emission as an image XObject (placed by the CTM, under the
/// current clip). An `/ImageMask true` stencil is painted in the current fill
/// colour; an unsupported codec or colour space yields nothing.
void emit_inline_image(const GraphicsOperator &op, const Resources &resources,
                       GraphicsState &state, std::vector<PageElement> &out) {
  if (op.arguments.size() < 2 || !op.arguments[0].is_dictionary() ||
      !op.arguments[1].is_string()) {
    return;
  }
  const Dictionary dictionary =
      normalize_inline_image(op.arguments[0].as_dictionary());

  const auto width = static_cast<std::int32_t>(
      dictionary.get("Width").as_integer_opt().value_or(0));
  const auto height = static_cast<std::int32_t>(
      dictionary.get("Height").as_integer_opt().value_or(0));
  const std::vector<double> decode_array = as_reals(dictionary.get("Decode"));

  // An inline `/ImageMask true` stencil: decode the 1-bpc bitmap and paint it
  // in the current fill colour, as for a stencil image XObject (ISO
  // 32000-1 8.9.7).
  if (dictionary.get("ImageMask").as_bool_opt().value_or(false)) {
    DecodeResult mask =
        decode(dictionary.get("Filter"), dictionary.get("DecodeParms"),
               op.arguments[1].as_string());
    if (mask.stopped_at_filter.has_value()) {
      return;
    }
    std::string png = encode_stencil_png(
        mask.data, width, height, color_to_rgb(state.current().other_color),
        decode_array);
    if (png.empty()) {
      return;
    }
    ImageElement image;
    image.transform = state.current().general.transform_matrix;
    image.clip = state.current().clip;
    image.data = std::move(png);
    image.mime = "image/png";
    out.push_back(std::move(image));
    return;
  }

  const std::shared_ptr<ColorSpaceDef> color_space =
      resolve_inline_color_space(dictionary.get("ColorSpace"), resources);
  const auto bits_per_component = static_cast<std::int32_t>(
      dictionary.get("BitsPerComponent").as_integer_opt().value_or(8));

  std::optional<EncodedImage> encoded =
      encode_image(op.arguments[1].as_string(), dictionary.get("Filter"),
                   dictionary.get("DecodeParms"), width, height,
                   bits_per_component, color_space.get(), decode_array);
  if (!encoded.has_value()) {
    return;
  }

  ImageElement image;
  image.transform = state.current().general.transform_matrix;
  image.clip = state.current().clip;
  image.data = std::move(encoded->data);
  image.mime = std::move(encoded->mime);
  out.push_back(std::move(image));
}

/// Emit a `ShadingElement` for the `sh` operator (ISO 32000-1 8.7.4.2): flood
/// the named `/Shading` over the current clip region. The shading is mapped to
/// user space by the CTM in force; unsupported shadings (parsed to nothing)
/// produce no element.
void emit_shading(const GraphicsOperator &op, const Resources &resources,
                  GraphicsState &state, std::vector<PageElement> &out) {
  if (op.arguments.empty() || !op.arguments.at(0).is_name()) {
    return;
  }
  const auto it = resources.shading.find(op.arguments.at(0).as_name());
  if (it == resources.shading.end() || it->second == nullptr) {
    return;
  }
  ShadingElement element;
  element.shading = it->second.get();
  element.transform = state.current().general.transform_matrix;
  element.clip = state.current().clip;
  out.push_back(std::move(element));
}

/// Form XObjects currently being rendered, by element identity. The parser
/// represents the file faithfully, so the XObject graph may contain cycles
/// (the spec forbids them — ISO 32000-1 8.10.1 — but real files err); this set
/// breaks them at render time.
using ActiveForms = std::set<const XObject *>;

void run_content(const std::string &content, const Resources &resources,
                 GraphicsState &state, std::vector<PageElement> &out,
                 const Logger &logger, std::set<std::string> &warned,
                 ActiveForms &active, MarkedContentStack &marked,
                 std::optional<Pen> &pen);

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
/// `/BBox` clips the form's content. An image XObject emits an `ImageElement`
/// (when its codec passes through); unknown subtypes are skipped, and a form
/// already on the render stack is skipped (cycle guard).
void invoke_x_object(const std::string &name, const Resources &resources,
                     GraphicsState &state, std::vector<PageElement> &out,
                     const Logger &logger, std::set<std::string> &warned,
                     ActiveForms &active, MarkedContentStack &marked,
                     std::optional<Pen> &pen) {
  const auto it = resources.x_object.find(name);
  if (it == resources.x_object.end()) {
    if (warned.insert("xobject:" + name).second) {
      ODR_WARNING(logger, "pdf: unknown XObject resource '" + name + "'");
    }
    return;
  }

  const XObject *x_object = it->second;
  if (x_object->subtype == XObject::Subtype::image) {
    // An image is placed by the CTM in effect (its unit square maps to user
    // space), under the current clip.
    ImageElement image;
    image.transform = state.current().general.transform_matrix;
    image.clip = state.current().clip;
    if (x_object->stencil_mask) {
      // A 1-bpc stencil painted in the current fill colour (resolved here, not
      // at parse time, since the colour lives in the graphics state).
      std::string png = encode_stencil_png(
          x_object->stencil_samples, x_object->stencil_width,
          x_object->stencil_height, color_to_rgb(state.current().other_color),
          x_object->stencil_decode);
      if (png.empty()) {
        return;
      }
      image.data = std::move(png);
      image.mime = "image/png";
    } else if (!x_object->image_data.empty()) {
      // A normal image: bytes ready for the browser (JPEG pass-through or a
      // PNG-encoded raster). Codecs we cannot hand off carry none, so skip.
      image.data = x_object->image_data;
      image.mime = x_object->image_mime;
    } else {
      return;
    }
    out.push_back(std::move(image));
    return;
  }
  if (x_object->subtype != XObject::Subtype::form) {
    return; // unknown subtypes are inexecutable
  }
  if (!active.insert(x_object).second) {
    ODR_WARNING(logger, "pdf: cyclic form XObject invocation, skipping");
    return; // already on the render stack
  }

  state.save();
  state.concat_matrix(x_object->matrix);
  // `/BBox` clips the form's content to its bounding box (ISO 32000-1 8.10.2),
  // mapped through the (now form-matrix-concatenated) CTM. Scoped by the
  // surrounding save/restore.
  if (x_object->bbox.has_value()) {
    const std::array<double, 4> &b = *x_object->bbox;
    state.clip_bounding_box(b[0], b[1], b[2], b[3]);
  }
  const Resources &scope =
      x_object->resources != nullptr ? *x_object->resources : resources;
  // A form's marked content must be self-balanced; truncate back to the entry
  // depth afterwards so an unbalanced form cannot corrupt the enclosing scope.
  const std::size_t depth = marked.size();
  run_content(x_object->content, scope, state, out, logger, warned, active,
              marked, pen);
  marked.resize(depth);
  state.restore();
  active.erase(x_object);
}

void run_content(const std::string &content, const Resources &resources,
                 GraphicsState &state, std::vector<PageElement> &out,
                 const Logger &logger, std::set<std::string> &warned,
                 ActiveForms &active, MarkedContentStack &marked,
                 std::optional<Pen> &pen) {
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
      show(out, state, marked, pen, op.arguments.at(0).as_string(), font);
    } break;
    case GraphicsOperatorType::show_text_manual_spacing: { // TJ
      Font *font =
          lookup_font(resources, state.current().text.font, logger, warned);
      const GraphicsState::Text &text = state.current().text;
      for (const Object &item : op.arguments.at(0).as_array()) {
        if (item.is_string()) {
          show(out, state, marked, pen, item.as_string(), font);
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
      show(out, state, marked, pen, op.arguments.at(2).as_string(), font);
    } break;
    case GraphicsOperatorType::draw_object: // Do
      invoke_x_object(op.arguments.at(0).as_string(), resources, state, out,
                      logger, warned, active, marked, pen);
      break;
    case GraphicsOperatorType::begin_inline_image_data: // BI … ID … EI
      emit_inline_image(op, resources, state, out);
      break;

    case GraphicsOperatorType::stroke: // S
      paint_path(out, resources, state, false, true, false, false);
      break;
    case GraphicsOperatorType::close_stroke: // s
      paint_path(out, resources, state, false, true, false, true);
      break;
    case GraphicsOperatorType::fill_nonzero: // f, F
      paint_path(out, resources, state, true, false, false, false);
      break;
    case GraphicsOperatorType::fill_evenodd: // f*
      paint_path(out, resources, state, true, false, true, false);
      break;
    case GraphicsOperatorType::fill_nonzero_stroke: // B
      paint_path(out, resources, state, true, true, false, false);
      break;
    case GraphicsOperatorType::fill_evenodd_stroke: // B*
      paint_path(out, resources, state, true, true, true, false);
      break;
    case GraphicsOperatorType::close_fill_nonzero_stroke: // b
      paint_path(out, resources, state, true, true, false, true);
      break;
    case GraphicsOperatorType::close_fill_evenodd_stroke: // b*
      paint_path(out, resources, state, true, true, true, true);
      break;
    case GraphicsOperatorType::set_clipping_path_shading: // sh
      emit_shading(op, resources, state, out);
      break;
    case GraphicsOperatorType::end_path: // n
      // Path painted with no marks — its only role is to install a pending
      // `W`/`W*` clip (ISO 32000-1 8.5.4); commit it, then discard the
      // geometry.
      state.commit_clip();
      state.clear_path();
      break;

    // Colour-space and general-colour operators are resolved here (not in
    // `GraphicsState::execute`) because they consult the `/ColorSpace`
    // resources. The device colour operators stay in `execute`.
    case GraphicsOperatorType::set_other_color_space: // cs
      if (!op.arguments.empty()) {
        set_color_space(state.current().other_color,
                        op.arguments.at(0).as_string(), resources);
      }
      break;
    case GraphicsOperatorType::set_stroke_color_space: // CS
      if (!op.arguments.empty()) {
        set_color_space(state.current().stroke_color,
                        op.arguments.at(0).as_string(), resources);
      }
      break;
    case GraphicsOperatorType::set_other_color:      // sc
    case GraphicsOperatorType::set_other_color_name: // scn
      set_color(state.current().other_color, op);
      break;
    case GraphicsOperatorType::set_stroke_color:      // SC
    case GraphicsOperatorType::set_stroke_color_name: // SCN
      set_color(state.current().stroke_color, op);
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

std::vector<pdf::PageElement> pdf::extract_page(const std::string &content,
                                                const Resources &resources,
                                                const Logger &logger) {
  std::vector<PageElement> result;
  std::set<std::string> warned;
  ActiveForms active;
  MarkedContentStack marked;
  std::optional<Pen> pen;
  GraphicsState state;

  run_content(content, resources, state, result, logger, warned, active, marked,
              pen);

  return result;
}

std::vector<pdf::TextElement> pdf::extract_text(const std::string &content,
                                                const Resources &resources,
                                                const Logger &logger) {
  std::vector<TextElement> result;
  for (PageElement &element : extract_page(content, resources, logger)) {
    if (auto *text = std::get_if<TextElement>(&element)) {
      result.push_back(std::move(*text));
    }
  }
  return result;
}

} // namespace odr::internal
