#include <odr/internal/pdf/pdf_graphics_state.hpp>

#include <odr/internal/pdf/pdf_graphics_operator.hpp>
#include <odr/internal/util/map_util.hpp>

#include <iostream>
#include <unordered_map>

namespace odr::internal::pdf {

namespace {

ColorSpace color_space_name_to_enum(const std::string &name) {
  static std::unordered_map<std::string, ColorSpace> mapping{
      {"grey", ColorSpace::device_grey},
      {"rgb", ColorSpace::device_rgb},
      {"cmyk", ColorSpace::device_cmyk},
  };

  return util::map::lookup_default(mapping, name, ColorSpace::unknown);
}

util::math::Transform2D matrix_from_args(const GraphicsOperator &op) {
  return {op.arguments.at(0).as_real(), op.arguments.at(1).as_real(),
          op.arguments.at(2).as_real(), op.arguments.at(3).as_real(),
          op.arguments.at(4).as_real(), op.arguments.at(5).as_real()};
}

} // namespace

GraphicsState::GraphicsState() { stack.emplace_back(); }

GraphicsState::State &GraphicsState::current() { return stack.back(); }

const GraphicsState::State &GraphicsState::current() const {
  return stack.back();
}

util::math::Transform2D GraphicsState::text_placement_transform() const {
  const Text &text = current().text;
  // text rendering matrix without the font size (ISO 32000-1 9.4.4): the font
  // size scales x and y, horizontal scaling scales x only, rise offsets y.
  const util::math::Transform2D params =
      util::math::Transform2D::translation_scaling(
          0, text.rise, text.horizontal_scaling / 100.0, 1);
  return params * text.matrix * current().general.transform_matrix;
}

void GraphicsState::next_line(const double tx, const double ty) {
  Text &text = current().text;
  text.line_matrix =
      util::math::Transform2D::translation(tx, ty) * text.line_matrix;
  text.matrix = text.line_matrix;
}

void GraphicsState::advance_text(double tx, double ty) {
  Text &text = current().text;
  text.matrix = util::math::Transform2D::translation(tx, ty) * text.matrix;
}

void GraphicsState::execute(const GraphicsOperator &op) {
  switch (op.type) {
  case GraphicsOperatorType::save_state:
    stack.push_back(stack.back());
    break;
  case GraphicsOperatorType::restore_state:
    stack.pop_back();
    break;

  case GraphicsOperatorType::set_matrix:
    // `cm` concatenates: CTM = matrix * CTM (ISO 32000-1 8.4.4).
    current().general.transform_matrix =
        matrix_from_args(op) * current().general.transform_matrix;
    break;

  case GraphicsOperatorType::set_line_width:
    current().general.line_width = op.arguments.at(0).as_real();
    break;
  case GraphicsOperatorType::set_cap_style:
    current().general.cap_style =
        static_cast<int>(op.arguments.at(0).as_integer());
    break;
  case GraphicsOperatorType::set_join_style:
    current().general.join_style =
        static_cast<int>(op.arguments.at(0).as_integer());
    break;
  case GraphicsOperatorType::set_miter_limit:
    current().general.miter_limit = op.arguments.at(0).as_real();
    break;
  case GraphicsOperatorType::set_dash_pattern:
    std::cout << "dash pattern not implemented" << '\n';
    break;
  case GraphicsOperatorType::set_color_rendering_intent:
    current().general.color_rendering_intent = op.arguments.at(0).as_name();
    break;
  case GraphicsOperatorType::set_flatness_tolerance:
    current().general.flatness_tolerance = op.arguments.at(0).as_real();
    break;
  case GraphicsOperatorType::set_graphics_state_parameters:
    current().general.graphics_state_parameters =
        op.arguments.at(0).as_string();
    break;

  case GraphicsOperatorType::path_move_to:
    for (int i = 0; i < 2; ++i) {
      current().path.current_position.at(i) = op.arguments.at(i).as_real();
    }
    break;
  case GraphicsOperatorType::path_line_to:
    for (int i = 0; i < 2; ++i) {
      current().path.current_position.at(i) = op.arguments.at(i).as_real();
    }
    break;
  case GraphicsOperatorType::path_cubic_bezier_to:
    for (int i = 0; i < 2; ++i) {
      current().path.current_position.at(i) = op.arguments.at(i + 4).as_real();
    }
    break;
  case GraphicsOperatorType::path_cubic_bezier_0eq1_to:
    for (int i = 0; i < 2; ++i) {
      current().path.current_position.at(i) = op.arguments.at(i + 2).as_real();
    }
    break;
  case GraphicsOperatorType::path_cubic_bezier_2eq3_to:
    for (int i = 0; i < 2; ++i) {
      current().path.current_position.at(i) = op.arguments.at(i + 2).as_real();
    }
    break;

  case GraphicsOperatorType::set_text_char_spacing:
    current().text.char_spacing = op.arguments.at(0).as_real();
    break;
  case GraphicsOperatorType::set_text_word_spacing:
    current().text.word_spacing = op.arguments.at(0).as_real();
    break;
  case GraphicsOperatorType::set_text_horizontal_scaling:
    current().text.horizontal_scaling = op.arguments.at(0).as_real();
    break;
  case GraphicsOperatorType::set_text_leading:
    current().text.leading = op.arguments.at(0).as_real();
    break;
  case GraphicsOperatorType::set_text_font_size:
    current().text.font = op.arguments.at(0).as_string();
    current().text.size = op.arguments.at(1).as_real();
    break;
  case GraphicsOperatorType::set_text_rendering_mode:
    current().text.rendering_mode =
        static_cast<int>(op.arguments.at(0).as_integer());
    break;
  case GraphicsOperatorType::set_text_rise:
    current().text.rise = op.arguments.at(0).as_real();
    break;

  case GraphicsOperatorType::begin_text:
    // BT initializes both the text matrix and the text line matrix to identity.
    current().text.matrix = util::math::Transform2D();
    current().text.line_matrix = util::math::Transform2D();
    break;
  case GraphicsOperatorType::text_next_line_relative: // Td
    next_line(op.arguments.at(0).as_real(), op.arguments.at(1).as_real());
    break;
  case GraphicsOperatorType::text_next_line_relative_leading: // TD
    current().text.leading = -op.arguments.at(1).as_real();
    next_line(op.arguments.at(0).as_real(), op.arguments.at(1).as_real());
    break;
  case GraphicsOperatorType::set_text_matrix: // Tm
    current().text.matrix = matrix_from_args(op);
    current().text.line_matrix = current().text.matrix;
    break;
  case GraphicsOperatorType::text_next_line: // T*
    next_line(0, -current().text.leading);
    break;
  case GraphicsOperatorType::show_text_next_line: // ' : T* then show
    next_line(0, -current().text.leading);
    break;
  case GraphicsOperatorType::show_text_next_line_set_spacing:
    // " : aw ac string -> set word/char spacing, T*, then show
    current().text.word_spacing = op.arguments.at(0).as_real();
    current().text.char_spacing = op.arguments.at(1).as_real();
    next_line(0, -current().text.leading);
    break;

  case GraphicsOperatorType::set_stroke_color_space:
    current().stroke_color.space =
        color_space_name_to_enum(op.arguments.at(0).as_string());
    break;
  case GraphicsOperatorType::set_stroke_color:
    // TODO
    std::cout << "stroke color not implemented" << '\n';
    break;
  case GraphicsOperatorType::set_stroke_color_name:
    // TODO
    std::cout << "stroke color name not implemented" << '\n';
    break;
  case GraphicsOperatorType::set_stroke_grey_color:
    current().stroke_color.grey = op.arguments.at(0).as_real();
    break;
  case GraphicsOperatorType::set_stroke_rgb_color:
    for (int i = 0; i < 3; ++i) {
      current().stroke_color.rgb.at(i) = op.arguments.at(i).as_real();
    }
    break;
  case GraphicsOperatorType::set_stroke_cmyk_color:
    for (int i = 0; i < 4; ++i) {
      current().stroke_color.cmyk.at(i) = op.arguments.at(i).as_real();
    }
    break;

  case GraphicsOperatorType::set_other_color_space:
    current().other_color.space =
        color_space_name_to_enum(op.arguments.at(0).as_string());
    break;
  case GraphicsOperatorType::set_other_color:
    // TODO
    std::cout << "other color not implemented" << '\n';
    break;
  case GraphicsOperatorType::set_other_color_name:
    // TODO
    std::cout << "other color name not implemented" << '\n';
    break;
  case GraphicsOperatorType::set_other_grey_color:
    current().other_color.grey = op.arguments.at(0).as_real();
    break;
  case GraphicsOperatorType::set_other_rgb_color:
    for (int i = 0; i < 3; ++i) {
      current().other_color.rgb.at(i) = op.arguments.at(i).as_real();
    }
    break;
  case GraphicsOperatorType::set_other_cmyk_color:
    for (int i = 0; i < 4; ++i) {
      current().other_color.cmyk.at(i) = op.arguments.at(i).as_real();
    }
    break;

  case GraphicsOperatorType::set_glyph_width:
    for (int i = 0; i < 2; ++i) {
      current().text.glyph_width.at(i) = op.arguments.at(i).as_real();
    }
    break;
  case GraphicsOperatorType::set_glyph_width_bounding_box:
    for (int i = 0; i < 2; ++i) {
      current().text.glyph_width.at(i) = op.arguments.at(i).as_real();
    }
    for (int i = 0; i < 4; ++i) {
      current().text.glyph_bounding_box.at(i) =
          op.arguments.at(i + 2).as_real();
    }
    break;

  default:
    break;
  }
}

} // namespace odr::internal::pdf
