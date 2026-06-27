#include <odr/internal/pdf/pdf_graphics_operator_parser.hpp>

#include <odr/internal/pdf/pdf_graphics_operator.hpp>
#include <odr/internal/util/map_util.hpp>

#include <cstdint>
#include <iostream>
#include <optional>
#include <unordered_map>

namespace odr::internal::pdf {

namespace {

GraphicsOperatorType operator_name_to_type(const std::string &name) {
  static std::unordered_map<std::string, GraphicsOperatorType> mapping = {
      {"q", GraphicsOperatorType::save_state},
      {"Q", GraphicsOperatorType::restore_state},

      {"cm", GraphicsOperatorType::set_matrix},

      {"w", GraphicsOperatorType::set_line_width},
      {"J", GraphicsOperatorType::set_cap_style},
      {"j", GraphicsOperatorType::set_join_style},
      {"M", GraphicsOperatorType::set_miter_limit},
      {"d", GraphicsOperatorType::set_dash_pattern},
      {"ri", GraphicsOperatorType::set_color_rendering_intent},
      {"i", GraphicsOperatorType::set_flatness_tolerance},
      {"gm", GraphicsOperatorType::set_graphics_state_parameters},

      {"Do", GraphicsOperatorType::draw_object},
      {"BI", GraphicsOperatorType::begin_inline_image},
      {"ID", GraphicsOperatorType::begin_inline_image_data},
      {"EI", GraphicsOperatorType::end_inline_image},

      {"m", GraphicsOperatorType::path_move_to},
      {"l", GraphicsOperatorType::path_line_to},
      {"c", GraphicsOperatorType::path_cubic_bezier_to},
      {"v", GraphicsOperatorType::path_cubic_bezier_0eq1_to},
      {"y", GraphicsOperatorType::path_cubic_bezier_2eq3_to},
      {"h", GraphicsOperatorType::close_path},
      {"re", GraphicsOperatorType::rectangle},

      {"W", GraphicsOperatorType::set_clipping_nonzero},
      {"W*", GraphicsOperatorType::set_clipping_evenodd},
      {"sh", GraphicsOperatorType::set_clipping_path_shading},

      {"S", GraphicsOperatorType::stroke},
      {"s", GraphicsOperatorType::close_stroke},
      {"f", GraphicsOperatorType::fill_nonzero},
      {"F", GraphicsOperatorType::fill_nonzero},
      {"f*", GraphicsOperatorType::fill_evenodd},
      {"B", GraphicsOperatorType::fill_nonzero_stroke},
      {"B*", GraphicsOperatorType::fill_evenodd_stroke},
      {"b", GraphicsOperatorType::close_fill_nonzero_stroke},
      {"b*", GraphicsOperatorType::close_fill_evenodd_stroke},
      {"n", GraphicsOperatorType::end_path},

      {"BT", GraphicsOperatorType::begin_text},
      {"ET", GraphicsOperatorType::end_text},

      {"Tc", GraphicsOperatorType::set_text_char_spacing},
      {"Tw", GraphicsOperatorType::set_text_word_spacing},
      {"Tz", GraphicsOperatorType::set_text_horizontal_scaling},
      {"TL", GraphicsOperatorType::set_text_leading},
      {"Tf", GraphicsOperatorType::set_text_font_size},
      {"Tr", GraphicsOperatorType::set_text_rendering_mode},
      {"Ts", GraphicsOperatorType::set_text_rise},

      {"Td", GraphicsOperatorType::text_next_line_relative},
      {"TD", GraphicsOperatorType::text_next_line_relative_leading},
      {"Tm", GraphicsOperatorType::set_text_matrix},
      {"T*", GraphicsOperatorType::text_next_line},

      {"Tj", GraphicsOperatorType::show_text},
      {"TJ", GraphicsOperatorType::show_text_manual_spacing},
      {"'", GraphicsOperatorType::show_text_next_line},
      {"\"", GraphicsOperatorType::show_text_next_line_set_spacing},

      {"CS", GraphicsOperatorType::set_stroke_color_space},
      {"SC", GraphicsOperatorType::set_stroke_color},
      {"SCN", GraphicsOperatorType::set_stroke_color_name},
      {"G", GraphicsOperatorType::set_stroke_grey_color},
      {"RG", GraphicsOperatorType::set_stroke_rgb_color},
      {"K", GraphicsOperatorType::set_stroke_cmyk_color},

      {"cs", GraphicsOperatorType::set_other_color_space},
      {"sc", GraphicsOperatorType::set_other_color},
      {"scn", GraphicsOperatorType::set_other_color_name},
      {"g", GraphicsOperatorType::set_other_grey_color},
      {"rg", GraphicsOperatorType::set_other_rgb_color},
      {"k", GraphicsOperatorType::set_other_cmyk_color},

      {"d0", GraphicsOperatorType::set_glyph_width},
      {"d1", GraphicsOperatorType::set_glyph_width_bounding_box},

      {"MP", GraphicsOperatorType::marked_content_point},
      {"DP", GraphicsOperatorType::point_with_props},
      {"BMC", GraphicsOperatorType::begin_marked_content_seq},
      {"BDC", GraphicsOperatorType::begin_marked_content_seq_props},
      {"EMC", GraphicsOperatorType::end_marked_content_seq},

      {"BX", GraphicsOperatorType::begin_compat_sec},
      {"EX", GraphicsOperatorType::end_compat_sec},
  };

  return util::map::lookup_default(mapping, name,
                                   GraphicsOperatorType::unknown);
}

// Fetch an inline image dictionary entry by either its abbreviated or its long
// key (8.9.7, Table 93), since both spellings are permitted.
const Object &inline_image_entry(const Dictionary &dictionary,
                                 const std::string &abbrev,
                                 const std::string &full) {
  if (const Object &value = dictionary.get(abbrev); !value.is_null()) {
    return value;
  }
  return dictionary.get(full);
}

// Sample components of an inline image's colour space, but only for the device
// spaces that are self-contained in inline content. Named spaces (page
// `/ColorSpace` resources) and `/Indexed` arrays are not resolved here.
std::optional<int> inline_color_components(const Object &color_space) {
  if (!color_space.is_name()) {
    return std::nullopt;
  }
  const std::string &name = color_space.as_name();
  if (name == "G" || name == "DeviceGray") {
    return 1;
  }
  if (name == "RGB" || name == "DeviceRGB") {
    return 3;
  }
  if (name == "CMYK" || name == "DeviceCMYK") {
    return 4;
  }
  return std::nullopt;
}

// Byte length of an inline image's raw data, derived from its sample geometry —
// but only when that length is knowable: the image must be unfiltered (a
// filtered payload's size is unknown until decoded) and its colour space must
// be a device space. Returns `nullopt` otherwise, leaving the caller to scan
// for the `EI` terminator.
std::optional<std::size_t>
inline_image_raw_length(const Dictionary &dictionary) {
  if (!inline_image_entry(dictionary, "F", "Filter").is_null()) {
    return std::nullopt;
  }

  int components;
  int bits_per_component;
  if (inline_image_entry(dictionary, "IM", "ImageMask")
          .as_bool_opt()
          .value_or(false)) {
    // A stencil mask is one bit per sample (8.9.6.2).
    components = 1;
    bits_per_component = 1;
  } else {
    const std::optional<int> resolved = inline_color_components(
        inline_image_entry(dictionary, "CS", "ColorSpace"));
    if (!resolved.has_value()) {
      return std::nullopt;
    }
    components = *resolved;
    bits_per_component = static_cast<int>(
        inline_image_entry(dictionary, "BPC", "BitsPerComponent")
            .as_integer_opt()
            .value_or(8));
  }

  const std::int64_t width =
      inline_image_entry(dictionary, "W", "Width").as_integer_opt().value_or(0);
  const std::int64_t height = inline_image_entry(dictionary, "H", "Height")
                                  .as_integer_opt()
                                  .value_or(0);
  if (width <= 0 || height <= 0 || bits_per_component <= 0) {
    return std::nullopt;
  }

  // Each sample row is padded to a byte boundary (8.9.5.2).
  const std::size_t bits_per_row =
      static_cast<std::size_t>(width) * components * bits_per_component;
  const std::size_t bytes_per_row = (bits_per_row + 7) / 8;
  return bytes_per_row * static_cast<std::size_t>(height);
}

} // namespace

using char_type = std::streambuf::char_type;
using int_type = std::streambuf::int_type;
static constexpr int_type eof = std::streambuf::traits_type::eof();

GraphicsOperatorParser::GraphicsOperatorParser(std::istream &in)
    : m_parser(in) {}

std::istream &GraphicsOperatorParser::in() { return m_parser.in(); }

std::streambuf &GraphicsOperatorParser::sb() { return m_parser.sb(); }

std::string GraphicsOperatorParser::read_operator_name() {
  std::string result;

  while (true) {
    const int_type c = m_parser.geti();

    if (c == eof) {
      return result;
    }
    if (c == ' ' || c == '\n' || c == '/' || c == '<' || c == '[') {
      return result;
    }

    m_parser.bumpc();
    result += static_cast<char_type>(c);
  }
}

GraphicsOperator GraphicsOperatorParser::read_operator() {
  GraphicsOperator result;

  std::string operator_name;
  while (true) {
    if (m_parser.peek_number()) {
      std::visit([&](auto v) { result.arguments.emplace_back(v); },
                 m_parser.read_integer_or_real());
    } else if (m_parser.peek_name()) {
      result.arguments.emplace_back(m_parser.read_name());
    } else if (m_parser.peek_string()) {
      std::visit([&](auto s) { result.arguments.emplace_back(std::move(s)); },
                 m_parser.read_string());
    } else if (m_parser.peek_array()) {
      result.arguments.emplace_back(m_parser.read_array());
    } else if (m_parser.peek_dictionary()) {
      result.arguments.emplace_back(m_parser.read_dictionary());
    } else {
      // A bareword here is either a `true`/`false` literal (a value inside an
      // inline image dictionary, 8.9.7) or the operator name that ends the
      // arguments. `peek_boolean` cannot disambiguate these because operators
      // such as `f` (fill) or `Tj` share their leading character with the
      // boolean keywords, so read the whole word and compare it.
      operator_name = read_operator_name();
      if (operator_name == "true") {
        result.arguments.emplace_back(Boolean(true));
      } else if (operator_name == "false") {
        result.arguments.emplace_back(Boolean(false));
      } else {
        break;
      }
    }
    m_parser.skip_whitespace();
  }

  result.type = operator_name_to_type(operator_name);
  if (result.type == GraphicsOperatorType::unknown) {
    std::cerr << "unknown operator: " << operator_name << '\n';
  }

  // `BI <key val …> ID <bytes> EI` is one inline image (8.9.7). The key/value
  // pairs were just read as this operator's arguments; fold them into a
  // dictionary and capture the raw bytes (also stopping them from being
  // mis-tokenized as operators), so the operator carries `[dict, bytes]`.
  if (result.type == GraphicsOperatorType::begin_inline_image_data) {
    Dictionary dictionary = read_inline_image_dictionary(result.arguments);
    std::string data = read_inline_image_data(dictionary);
    result.arguments.clear();
    result.arguments.emplace_back(std::move(dictionary));
    result.arguments.emplace_back(StandardString(std::move(data)));
  }

  m_parser.skip_whitespace();

  return result;
}

Dictionary GraphicsOperatorParser::read_inline_image_dictionary(
    const std::vector<Object> &arguments) {
  // The inline dictionary is a flat run of name/value pairs (8.9.7).
  // Abbreviated keys are normalized to their long forms downstream.
  Dictionary dictionary;
  for (std::size_t i = 0; i + 1 < arguments.size(); i += 2) {
    if (arguments[i].is_name()) {
      dictionary[arguments[i].as_name()] = arguments[i + 1];
    }
  }
  return dictionary;
}

std::string
GraphicsOperatorParser::read_inline_image_data(const Dictionary &dictionary) {
  // Exactly one white-space character separates `ID` from the data (8.9.7).
  if (m_parser.geti() != eof) {
    m_parser.bumpc();
  }

  // When the byte length is knowable (an unfiltered device-colour image), read
  // exactly that many bytes. This is the only reliable terminator: raw samples
  // can contain the bytes `E I <white-space>`, which the scan below would
  // mistake for the real `EI` and so truncate the image (and corrupt the rest
  // of the page).
  if (const std::optional<std::size_t> length =
          inline_image_raw_length(dictionary)) {
    std::string data;
    data.reserve(*length);
    for (std::size_t i = 0; i < *length; ++i) {
      const int_type c = m_parser.geti();
      if (c == eof) {
        return data;
      }
      m_parser.bumpc();
      data += static_cast<char_type>(c);
    }
    // Consume the trailing `EI` (and the conventional white-space before it).
    m_parser.skip_whitespace();
    if (m_parser.geti() == 'E') {
      m_parser.bumpc();
      if (m_parser.geti() == 'I') {
        m_parser.bumpc();
      }
    }
    return data;
  }

  // Otherwise the length is not encoded (a filtered payload), so scan for the
  // `EI` terminator. `EI` also occurs inside the encoded bytes, so accept one
  // only when it is bracketed by white-space — preceded by it (the writer's
  // convention) and followed by it or eof — which makes a false match unlikely.
  // The terminator (and the white-space before it) is left out of the data.
  std::string data;
  while (true) {
    const int_type c = m_parser.geti();
    if (c == eof) {
      return data;
    }
    m_parser.bumpc();
    if (c == 'E') {
      const bool preceded_by_whitespace =
          data.empty() || ObjectParser::is_whitespace(data.back());
      if (preceded_by_whitespace && m_parser.geti() == 'I') {
        m_parser.bumpc();
        const int_type after = m_parser.geti();
        if (after == eof ||
            ObjectParser::is_whitespace(static_cast<char_type>(after))) {
          return data;
        }
        data += 'E';
        data += 'I';
        continue;
      }
      data += 'E';
      continue;
    }
    data += static_cast<char_type>(c);
  }
}

} // namespace odr::internal::pdf
