#ifndef ODR_INTERNAL_PDF_GRAPHICS_OPERATOR_HPP
#define ODR_INTERNAL_PDF_GRAPHICS_OPERATOR_HPP

#include <odr/internal/pdf/pdf_object.hpp>

#include <iosfwd>
#include <string>
#include <variant>
#include <vector>

namespace odr::internal::pdf {

// https://gendignoux.com/blog/images/pdf-graphics/cheat-sheet-by-nc-sa.png
enum class GraphicsOperatorType {
  unknown,

  save_state,
  restore_state,

  set_matrix,

  set_line_width,
  set_cap_style,
  set_join_style,
  set_miter_limit,
  set_dash_pattern,
  set_color_rendering_intent,
  set_flatness_tolerance,
  set_graphics_state_parameters,

  draw_object,
  begin_inline_image,
  begin_inline_image_data,
  end_inline_image,

  path_move_to,
  path_line_to,
  path_cubic_bezier_to,
  path_cubic_bezier_0eq1_to,
  path_cubic_bezier_2eq3_to,
  close_path,
  rectangle,

  set_clipping_nonzero,
  set_clipping_evenodd,
  set_clipping_path_shading,

  stroke,
  close_stroke,
  fill_nonzero,
  fill_evenodd,
  fill_nonzero_stroke,
  fill_evenodd_stroke,
  close_fill_nonzero_stroke,
  close_fill_evenodd_stroke,
  end_path,

  begin_text,
  end_text,

  set_text_char_spacing,
  set_text_word_spacing,
  set_text_horizontal_scaling,
  set_text_leading,
  set_text_font_size,
  set_text_rendering_mode,
  set_text_rise,

  text_next_line_relative,
  text_next_line_relative_leading,
  set_text_matrix,
  text_next_line,

  show_text,
  show_text_manual_spacing,
  show_text_next_line,
  show_text_next_line_set_spacing,

  set_stroke_color_space,
  set_stroke_color,
  set_stroke_color_name,
  set_stroke_grey_color,
  set_stroke_rgb_color,
  set_stroke_cmyk_color,

  set_other_color_space,
  set_other_color,
  set_other_color_name,
  set_other_grey_color,
  set_other_rgb_color,
  set_other_cmyk_color,

  set_glyph_width,
  set_glyph_width_bounding_box,

  marked_content_point,
  point_with_props,
  begin_marked_content_seq,
  begin_marked_content_seq_props,
  end_marked_content_seq,

  begin_compat_sec,
  end_compat_sec,
};

struct GraphicsOperator {
  using Argument = Object;
  using Arguments = std::vector<Argument>;

  GraphicsOperatorType type;
  Arguments arguments;
};

} // namespace odr::internal::pdf

#endif // ODR_INTERNAL_PDF_GRAPHICS_OPERATOR_HPP
