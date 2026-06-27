#include <odr/internal/pdf/pdf_graphics_state.hpp>

#include <odr/internal/pdf/pdf_graphics_operator.hpp>

namespace odr::internal::pdf {

namespace {

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
      util::math::Transform2D::scaling_translation(
          text.horizontal_scaling / 100.0, 1, 0, text.rise);
  return params * text.matrix * current().general.transform_matrix;
}

void GraphicsState::next_line(const double tx, const double ty) {
  Text &text = current().text;
  text.line_matrix =
      util::math::Transform2D::translation(tx, ty) * text.line_matrix;
  text.matrix = text.line_matrix;
}

void GraphicsState::advance_text(const double tx, const double ty) {
  Text &text = current().text;
  text.matrix = util::math::Transform2D::translation(tx, ty) * text.matrix;
}

std::array<double, 2> GraphicsState::to_user(const double x,
                                             const double y) const {
  return current().general.transform_matrix.apply(x, y);
}

void GraphicsState::append_segment(const PathSegment &segment) {
  if (path.empty()) {
    // A construction operator before any `m` is malformed; tolerate it by
    // opening a subpath at the current point.
    path.push_back(Subpath{m_current_point, {}, false});
  }
  path.back().segments.push_back(segment);
  m_current_point = segment.end;
}

void GraphicsState::path_move_to(const double x, const double y) {
  m_current_point = to_user(x, y);
  m_subpath_start = m_current_point;
  path.push_back(Subpath{m_current_point, {}, false});
}

void GraphicsState::path_line_to(const double x, const double y) {
  append_segment({PathSegment::Kind::line, {}, {}, to_user(x, y)});
}

void GraphicsState::path_cubic_to(const double x1, const double y1,
                                  const double x2, const double y2,
                                  const double x3, const double y3) {
  append_segment({PathSegment::Kind::cubic, to_user(x1, y1), to_user(x2, y2),
                  to_user(x3, y3)});
}

void GraphicsState::path_cubic_to_v(const double x2, const double y2,
                                    const double x3, const double y3) {
  append_segment({PathSegment::Kind::cubic, m_current_point, to_user(x2, y2),
                  to_user(x3, y3)});
}

void GraphicsState::path_cubic_to_y(const double x1, const double y1,
                                    const double x3, const double y3) {
  const std::array<double, 2> end = to_user(x3, y3);
  append_segment({PathSegment::Kind::cubic, to_user(x1, y1), end, end});
}

void GraphicsState::path_close() {
  if (path.empty()) {
    return;
  }
  path.back().closed = true;
  m_current_point = m_subpath_start;
}

void GraphicsState::path_rectangle(const double x, const double y,
                                   const double w, const double h) {
  // `re` appends a complete closed rectangle and leaves the current point at
  // its origin (ISO 32000-1 8.5.2.1).
  Subpath rect{to_user(x, y), {}, true};
  rect.segments.push_back({PathSegment::Kind::line, {}, {}, to_user(x + w, y)});
  rect.segments.push_back(
      {PathSegment::Kind::line, {}, {}, to_user(x + w, y + h)});
  rect.segments.push_back({PathSegment::Kind::line, {}, {}, to_user(x, y + h)});
  path.push_back(std::move(rect));
  m_current_point = to_user(x, y);
  m_subpath_start = m_current_point;
}

void GraphicsState::clear_path() {
  path.clear();
  m_current_point = {0, 0};
  m_subpath_start = {0, 0};
}

void GraphicsState::set_pending_clip(const bool even_odd) {
  m_pending_clip = even_odd ? PendingClip::even_odd : PendingClip::nonzero;
}

void GraphicsState::commit_clip() {
  if (m_pending_clip == PendingClip::none) {
    return;
  }
  current().clip.push_back(
      ClipPath{path, m_pending_clip == PendingClip::even_odd});
  m_pending_clip = PendingClip::none;
}

void GraphicsState::clip_bounding_box(const double x0, const double y0,
                                      const double x1, const double y1) {
  Subpath rect{to_user(x0, y0), {}, true};
  rect.segments.push_back({PathSegment::Kind::line, {}, {}, to_user(x1, y0)});
  rect.segments.push_back({PathSegment::Kind::line, {}, {}, to_user(x1, y1)});
  rect.segments.push_back({PathSegment::Kind::line, {}, {}, to_user(x0, y1)});
  current().clip.push_back(ClipPath{{std::move(rect)}, false});
}

void GraphicsState::save() { stack.push_back(stack.back()); }

void GraphicsState::restore() { stack.pop_back(); }

void GraphicsState::concat_matrix(const util::math::Transform2D &matrix) {
  // CTM = matrix * CTM (ISO 32000-1 8.4.4).
  current().general.transform_matrix =
      matrix * current().general.transform_matrix;
}

void GraphicsState::execute(const GraphicsOperator &op) {
  switch (op.type) {
  case GraphicsOperatorType::save_state:
    save();
    break;
  case GraphicsOperatorType::restore_state:
    restore();
    break;

  case GraphicsOperatorType::set_matrix:
    // `cm` concatenates: CTM = matrix * CTM (ISO 32000-1 8.4.4).
    concat_matrix(matrix_from_args(op));
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
  case GraphicsOperatorType::set_dash_pattern: {
    // `[a1 a2 …] phase d` (ISO 32000-1 8.4.3.6).
    Dash dash;
    if (op.arguments.at(0).is_array()) {
      for (const Object &item : op.arguments.at(0).as_array()) {
        dash.array.push_back(item.as_real());
      }
    }
    dash.phase = op.arguments.at(1).as_real();
    current().general.dash = std::move(dash);
  } break;
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

  case GraphicsOperatorType::path_move_to: // m
    path_move_to(op.arguments.at(0).as_real(), op.arguments.at(1).as_real());
    break;
  case GraphicsOperatorType::path_line_to: // l
    path_line_to(op.arguments.at(0).as_real(), op.arguments.at(1).as_real());
    break;
  case GraphicsOperatorType::path_cubic_bezier_to: // c
    path_cubic_to(op.arguments.at(0).as_real(), op.arguments.at(1).as_real(),
                  op.arguments.at(2).as_real(), op.arguments.at(3).as_real(),
                  op.arguments.at(4).as_real(), op.arguments.at(5).as_real());
    break;
  case GraphicsOperatorType::path_cubic_bezier_0eq1_to: // v
    path_cubic_to_v(op.arguments.at(0).as_real(), op.arguments.at(1).as_real(),
                    op.arguments.at(2).as_real(), op.arguments.at(3).as_real());
    break;
  case GraphicsOperatorType::path_cubic_bezier_2eq3_to: // y
    path_cubic_to_y(op.arguments.at(0).as_real(), op.arguments.at(1).as_real(),
                    op.arguments.at(2).as_real(), op.arguments.at(3).as_real());
    break;
  case GraphicsOperatorType::close_path: // h
    path_close();
    break;
  case GraphicsOperatorType::rectangle: // re
    path_rectangle(op.arguments.at(0).as_real(), op.arguments.at(1).as_real(),
                   op.arguments.at(2).as_real(), op.arguments.at(3).as_real());
    break;

  case GraphicsOperatorType::set_clipping_nonzero: // W
    set_pending_clip(false);
    break;
  case GraphicsOperatorType::set_clipping_evenodd: // W*
    set_pending_clip(true);
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
  case GraphicsOperatorType::text_next_line:      // T*
  case GraphicsOperatorType::show_text_next_line: // ' : T* then show (in
                                                  // extractor)
    next_line(0, -current().text.leading);
    break;
  case GraphicsOperatorType::show_text_next_line_set_spacing:
    // " : aw ac string -> set word/char spacing, T*, then show
    current().text.word_spacing = op.arguments.at(0).as_real();
    current().text.char_spacing = op.arguments.at(1).as_real();
    next_line(0, -current().text.leading);
    break;

  // The color-space (`cs`/`CS`) and general color (`sc`/`scn`/`SC`/`SCN`)
  // operators need the `/ColorSpace` resources to resolve, so they are handled
  // in `pdf_page_extractor` (which has the `Resources`), not here. The device
  // color operators below carry their components inline and clear any active
  // non-device color space.
  case GraphicsOperatorType::set_stroke_grey_color:
    current().stroke_color.space = ColorSpace::device_grey;
    current().stroke_color.grey = op.arguments.at(0).as_real();
    current().stroke_color.def = nullptr;
    break;
  case GraphicsOperatorType::set_stroke_rgb_color:
    current().stroke_color.space = ColorSpace::device_rgb;
    for (int i = 0; i < 3; ++i) {
      current().stroke_color.rgb.at(i) = op.arguments.at(i).as_real();
    }
    current().stroke_color.def = nullptr;
    break;
  case GraphicsOperatorType::set_stroke_cmyk_color:
    current().stroke_color.space = ColorSpace::device_cmyk;
    for (int i = 0; i < 4; ++i) {
      current().stroke_color.cmyk.at(i) = op.arguments.at(i).as_real();
    }
    current().stroke_color.def = nullptr;
    break;

  case GraphicsOperatorType::set_other_grey_color:
    current().other_color.space = ColorSpace::device_grey;
    current().other_color.grey = op.arguments.at(0).as_real();
    current().other_color.def = nullptr;
    break;
  case GraphicsOperatorType::set_other_rgb_color:
    current().other_color.space = ColorSpace::device_rgb;
    for (int i = 0; i < 3; ++i) {
      current().other_color.rgb.at(i) = op.arguments.at(i).as_real();
    }
    current().other_color.def = nullptr;
    break;
  case GraphicsOperatorType::set_other_cmyk_color:
    current().other_color.space = ColorSpace::device_cmyk;
    for (int i = 0; i < 4; ++i) {
      current().other_color.cmyk.at(i) = op.arguments.at(i).as_real();
    }
    current().other_color.def = nullptr;
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
