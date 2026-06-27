#include <odr/internal/pdf/pdf_graphics_operator_parser.hpp>

#include <odr/internal/pdf/pdf_graphics_operator.hpp>
#include <odr/internal/util/map_util.hpp>

#include <iostream>
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
    std::string data = read_inline_image_data();
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

std::string GraphicsOperatorParser::read_inline_image_data() {
  // Exactly one white-space character separates `ID` from the data (8.9.7).
  if (m_parser.geti() != eof) {
    m_parser.bumpc();
  }

  // The length is not encoded, so scan for the `EI` terminator. `EI` also
  // occurs inside the raw image bytes, so only accept one that is followed by
  // white-space or eof; otherwise keep the bytes and keep scanning. The
  // terminator (and the white-space convention before it) is left out of the
  // captured data.
  std::string data;
  while (true) {
    const int_type c = m_parser.geti();
    if (c == eof) {
      return data;
    }
    m_parser.bumpc();
    if (c == 'E') {
      if (m_parser.geti() == 'I') {
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
