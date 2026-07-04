#pragma once

#include <odr/internal/util/math_util.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace odr::internal::pdf {

struct GraphicsOperator;
struct ColorSpaceDef;

enum class ColorSpace {
  unknown,
  device_grey,
  device_rgb,
  device_cmyk,
};

/// Text rendering mode (`Tr`, ISO 32000-1 Table 106): how shown glyphs are
/// painted and/or added to the clipping path. The low two bits select the paint
/// (fill / stroke / both / none); the `_clip` modes additionally add the glyph
/// outlines to the clip. The integer values are the `Tr` operands.
enum class TextRenderingMode {
  fill = 0,
  stroke = 1,
  fill_stroke = 2,
  invisible = 3,
  fill_clip = 4,
  stroke_clip = 5,
  fill_stroke_clip = 6,
  clip = 7,
};

/// One segment of a subpath, in user space (the CTM is already applied at
/// construction time, ISO 32000-1 8.5.2.1). A line carries only `end`; a cubic
/// Bézier carries its two control points as well.
struct PathSegment {
  enum class Kind { line, cubic };
  Kind kind{Kind::line};
  std::array<double, 2> c1{};  // cubic control point 1 (unused for a line)
  std::array<double, 2> c2{};  // cubic control point 2 (unused for a line)
  std::array<double, 2> end{}; // segment endpoint
};

/// A subpath: a start point (from `m`/`re`), its segments, and whether it was
/// explicitly closed (`h` or a close-and-paint operator). Coordinates are user
/// space.
struct Subpath {
  std::array<double, 2> start{};
  std::vector<PathSegment> segments;
  bool closed{false};
};

/// One clipping region (ISO 32000-1 8.5.4): the path established by a `W`/`W*`
/// operator, in user space, plus the winding rule that fills it. The current
/// clip is the *intersection* of an ordered list of these — a path is visible
/// only where it lies inside every one.
struct ClipPath {
  std::vector<Subpath> subpaths;
  bool even_odd{false};
};

struct GraphicsState {
  /// Dash pattern (`d`): the on/off lengths and the starting phase, in user
  /// space. An empty array is a solid line (ISO 32000-1 8.4.3.6).
  struct Dash {
    std::vector<double> array;
    double phase{0};
  };

  struct General {
    // PDF initial graphics state (ISO 32000-1 Table 52): line width 1.0, butt
    // cap, miter join, miter limit 10.0. Defaulting these to 0 would emit
    // zero-width/invalid-miter path elements for streams that stroke without an
    // explicit `w`/`M`.
    double line_width{1};
    std::int32_t cap_style{};
    std::int32_t join_style{};
    double miter_limit{10};
    Dash dash;
    std::string color_rendering_intent;
    double flatness_tolerance{};
    std::string graphics_state_parameters;
    // Constant alpha (ISO 32000-1 11.6.4.4) set via an `/ExtGState` `ca`/`CA`:
    // the nonstroking (fill) and stroking opacity in [0,1]. Part of the general
    // graphics state, so `q`/`Q` scope them like the CTM. 1 = fully opaque.
    double fill_alpha{1};   // ca
    double stroke_alpha{1}; // CA
    // Blend mode (ISO 32000-1 11.3.5) set via an `/ExtGState` `/BM`: the PDF
    // separable/non-separable blend name (e.g. `Multiply`). Empty = `Normal`.
    std::string blend_mode;
    util::math::Transform2D transform_matrix; // CTM
  };

  struct Text {
    double char_spacing{0};         // Tc
    double word_spacing{0};         // Tw
    double horizontal_scaling{100}; // Tz, in percent (100 = normal)
    double leading{0};              // TL
    std::string font;               // Tf resource name
    double size{};                  // Tf size
    TextRenderingMode rendering_mode{TextRenderingMode::fill}; // Tr
    double rise{0};                                            // Ts
    util::math::Transform2D matrix;                            // Tm
    util::math::Transform2D line_matrix;                       // Tlm
    std::array<double, 2> glyph_width{};
    std::array<double, 4> glyph_bounding_box{};
  };

  struct Color {
    ColorSpace space{ColorSpace::device_grey};
    // TODO "color"
    double grey{};
    std::array<double, 3> rgb{};
    std::array<double, 4> cmyk{};
    /// The active non-device colour space set by `cs`/`CS` (a `/ColorSpace`
    /// resource: ICCBased, Separation, Indexed, …), owned by `Resources`. When
    /// set, `sc`/`scn` components are converted through it to the `rgb` above
    /// at the time the operator runs; null for a device colour space. Cleared
    /// by the device colour operators (`g`/`rg`/`k`).
    const ColorSpaceDef *def{nullptr};
    /// The resource name of the `/Pattern` selected by `scn`/`SCN` (a shading
    /// or tiling pattern), resolved against `Resources::pattern` at paint time.
    /// Empty for a plain colour; cleared by the device colour operators.
    std::string pattern;
  };

  struct State {
    General general;
    Text text;
    Color stroke_color;
    Color other_color;
    /// Current clipping path: the intersection of these regions (empty = the
    /// whole page). Part of the saved/restored state (ISO 32000-1 8.5.4), so
    /// `q`/`Q` and form-XObject invocation scope it like the CTM.
    std::vector<ClipPath> clip;
  };

  std::vector<State> stack;

  /// The path under construction. Unlike the rest of the state it is *not* part
  /// of the `q`/`Q` stack (ISO 32000-1 8.5.2): it accumulates across
  /// `m`/`l`/`c`/ `re`/… and is consumed (and cleared) by a path-painting
  /// operator.
  std::vector<Subpath> path;

  /// Nesting depth of Type3 char-proc rendering. A Type3 glyph is drawn by
  /// running a content stream that may itself show Type3 text, so this bounds
  /// pathological self-referential recursion. Deliberately *not* part of the
  /// `q`/`Q`-saved `State`: it tracks call-chain depth, not graphics state.
  std::int32_t type3_depth{0};

  GraphicsState();

  State &current();
  [[nodiscard]] const State &current() const;

  void execute(const GraphicsOperator &);

  /// Path construction. Operands are taken in the operator's coordinate space;
  /// each point is mapped through the current CTM and stored in user space.
  void path_move_to(double x, double y);
  void path_line_to(double x, double y);
  void path_cubic_to(double x1, double y1, double x2, double y2, double x3,
                     double y3);
  /// `v`: the first control point is the current point.
  void path_cubic_to_v(double x2, double y2, double x3, double y3);
  /// `y`: the second control point coincides with the endpoint.
  void path_cubic_to_y(double x1, double y1, double x3, double y3);
  void path_close();
  void path_rectangle(double x, double y, double w, double h);
  /// Close the current subpath (without the `h`-style "closed" mark) and drop
  /// the accumulated path, as every path-painting operator does on completion.
  void clear_path();

  /// Clipping (`W`/`W*`, ISO 32000-1 8.5.4). `set_pending_clip` records that
  /// the current path is to *become* a clip; `commit_clip` then installs it —
  /// as the intersection with the current clip — when the next painting (or
  /// `n`) operator completes. The path painted by that operator is still
  /// clipped by the *old* clip, so the caller snapshots the clip before calling
  /// `commit_clip`.
  void set_pending_clip(bool even_odd);
  void commit_clip();
  /// Intersect a rectangle (in the current CTM's space, e.g. a form's `/BBox`)
  /// into the current clip; the corners are mapped through the CTM.
  void clip_bounding_box(double x0, double y0, double x1, double y1);

  /// Push a copy of the current state (`q`).
  void save();
  /// Pop the current state (`Q`).
  void restore();
  /// Concatenate `matrix` onto the CTM (`CTM = matrix * CTM`), as `cm` does and
  /// as invoking a form XObject does with its `/Matrix`.
  void concat_matrix(const util::math::Transform2D &matrix);

  /// Text rendering transform *excluding* the font size: maps text space (1
  /// unit = 1 em at the current font size) to user space, with horizontal
  /// scaling and rise folded in. The font size is applied separately (as the
  /// rendered font-size), which keeps the run-vs-glyph mapping decision in the
  /// renderer.
  [[nodiscard]] util::math::Transform2D text_placement_transform() const;

  /// Advance the text matrix `Tm` by `(tx, ty)` in text space after showing
  /// glyphs (the text line matrix `Tlm` is unaffected).
  void advance_text(double tx, double ty);

private:
  /// Move to the start of a new text line: `Tlm = translate(tx, ty) * Tlm` and
  /// `Tm = Tlm` (the shared mechanic behind `Td`, `TD`, `T*`, `'`, `"`).
  void next_line(double tx, double ty);

  /// Map an operand point through the current CTM into user space.
  [[nodiscard]] std::array<double, 2> to_user(double x, double y) const;
  /// Append `segment` to the current subpath, starting one at the current point
  /// if a construction operator other than `m`/`re` opens the path (lenient).
  void append_segment(const PathSegment &segment);

  std::array<double, 2> m_current_point{0, 0}; // user space
  std::array<double, 2> m_subpath_start{0, 0}; // user space, for `h`/close

  /// A pending `W`/`W*` between path construction and the painting operator
  /// that installs it. Not part of the saved state: a `W` is always followed by
  /// a painting/`n` operator before any `q`/`Q` (ISO 32000-1 8.5.4).
  enum class PendingClip { none, nonzero, even_odd };
  PendingClip m_pending_clip{PendingClip::none};
};

} // namespace odr::internal::pdf
