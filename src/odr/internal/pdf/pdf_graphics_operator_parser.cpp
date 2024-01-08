#include <odr/internal/pdf/pdf_graphics_operator_parser.hpp>

#include <odr/internal/pdf/pdf_graphics_operator.hpp>
#include <odr/internal/util/map_util.hpp>

#include <unordered_map>

namespace odr::internal::pdf {

namespace {

GraphicsOperatorType operator_name_to_type(const std::string &name) {
  static std::unordered_map<std::string, GraphicsOperatorType> mapping = {
      {"w", GraphicsOperatorType::set_line_width},
      {"J", GraphicsOperatorType::set_cap_style},
      {"j", GraphicsOperatorType::set_join_style},
      {"M", GraphicsOperatorType::set_miter_limit},
      {"d", GraphicsOperatorType::set_dash_pattern},
      {"ri", GraphicsOperatorType::set_color_rendering_intent},
      {"i", GraphicsOperatorType::set_flatness_tolerance},
      {"gm", GraphicsOperatorType::set_graphics_state_parameters},

      {"q", GraphicsOperatorType::save_state},
      {"Q", GraphicsOperatorType::restore_state},
      {"cm", GraphicsOperatorType::set_matrix},

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
      {"TD", GraphicsOperatorType::text_next_line_text_leading},
      {"Tm", GraphicsOperatorType::set_text_matrix},
      {"T*", GraphicsOperatorType::text_next_line},

      {"Tj", GraphicsOperatorType::show_text},
      {"'", GraphicsOperatorType::next_line_show_text},
      {"\"", GraphicsOperatorType::set_spacing_next_line_show_text},
      {"TJ", GraphicsOperatorType::show_text_manual_spacing},

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

std::istream &GraphicsOperatorParser::in() const { return m_parser.in(); }

std::streambuf &GraphicsOperatorParser::sb() const { return m_parser.sb(); }

std::string GraphicsOperatorParser::read_operator_name() const {
  std::string result;

  while (true) {
    int_type c = sb().sgetc();

    if (c == eof) {
      in().setstate(std::ios::eofbit);
      return result;
    }
    if (c == ' ' || c == '\n' || c == '/' || c == '<' || c == '[') {
      return result;
    }

    sb().sbumpc();
    result += (char_type)c;
  }
}

SimpleArrayElement GraphicsOperatorParser::read_array_element() const {
  if (m_parser.peek_number()) {
    return std::visit([&](auto v) { return SimpleArrayElement(v); },
                      m_parser.read_integer_or_real());
  }
  if (m_parser.peek_name()) {
    return m_parser.read_name();
  }
  if (m_parser.peek_string()) {
    return std::visit([](auto s) { return SimpleArrayElement(std::move(s)); },
                      m_parser.read_string());
  }

  throw std::runtime_error("unknown element");
}

SimpleArray GraphicsOperatorParser::read_array() const {
  SimpleArray::Holder result;

  if (sb().sbumpc() != '[') {
    throw std::runtime_error("unexpected character");
  }
  m_parser.skip_whitespace();

  while (true) {
    if (int_type c = sb().sgetc(); c == ']') {
      sb().sbumpc();
      return SimpleArray(std::move(result));
    }
    SimpleArrayElement value = read_array_element();
    m_parser.skip_whitespace();

    result.emplace_back(std::move(value));
  }
}

GraphicsOperator GraphicsOperatorParser::read_operator() const {
  GraphicsOperator result;

  while (true) {
    if (m_parser.peek_number()) {
      std::visit([&](auto v) { result.arguments.push_back(v); },
                 m_parser.read_integer_or_real());
    } else if (m_parser.peek_name()) {
      result.arguments.push_back(m_parser.read_name());
    } else if (m_parser.peek_string()) {
      std::visit([&](auto s) { result.arguments.push_back(std::move(s)); },
                 m_parser.read_string());
    } else if (m_parser.peek_array()) {
      result.arguments.push_back(read_array());
    } else {
      m_parser.skip_whitespace();
      break;
    }
    m_parser.skip_whitespace();
  }

  result.type = operator_name_to_type(read_operator_name());
  if (result.type == GraphicsOperatorType::unknown) {
    throw std::runtime_error("unknown type");
  }

  m_parser.skip_whitespace();

  return result;
}

} // namespace odr::internal::pdf
