#pragma once

#include <odr/internal/util/math_util.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace odr::internal::pdf {

struct GraphicsOperator;
struct ColorSpaceDef;
struct SoftMask;

enum class ColorSpace {
  unknown,
  device_grey,
  device_rgb,
  device_cmyk,
};

/// Text rendering mode (`Tr`, ISO 32000-1 Table 106). The values are the `Tr`
/// operands; the low two bits select the paint, `>= 4` also clips.
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

/// One segment of a subpath, in user space (the CTM is applied at construction
/// time, ISO 32000-1 8.5.2.1).
struct PathSegment {
  enum class Kind { line, cubic };
  Kind kind{Kind::line};
  std::array<double, 2> c1{};  // cubic control point 1 (unused for a line)
  std::array<double, 2> c2{};  // cubic control point 2 (unused for a line)
  std::array<double, 2> end{}; // segment endpoint
};

/// A subpath from `m`/`re`, in user space. `closed` records an explicit `h` or
/// close-and-paint operator.
struct Subpath {
  std::array<double, 2> start{};
  std::vector<PathSegment> segments;
  bool closed{false};
};

/// One `W`/`W*` clipping region in user space (ISO 32000-1 8.5.4). The current
/// clip is the *intersection* of an ordered list of these.
struct ClipPath {
  std::vector<Subpath> subpaths;
  bool even_odd{false};
};

struct GraphicsState {
  /// Dash pattern (`d`, ISO 32000-1 8.4.3.6), user space; empty = solid.
  struct Dash {
    std::vector<double> array;
    double phase{0};
  };

  struct General {
    // Defaults are the PDF initial graphics state (ISO 32000-1 Table 52), not
    // zero: a stream that strokes without an explicit `w`/`M` must not produce
    // zero-width/invalid-miter paths.
    double line_width{1};
    std::int32_t cap_style{};
    std::int32_t join_style{};
    double miter_limit{10};
    Dash dash;
    std::string color_rendering_intent;
    double flatness_tolerance{};
    std::string graphics_state_parameters;
    double fill_alpha{1};   // `/ExtGState` ca (11.6.4.4)
    double stroke_alpha{1}; // `/ExtGState` CA
    std::string blend_mode; // `/ExtGState` /BM (11.3.5); empty = Normal
    /// `/ExtGState` `/SMask` (11.6.5.2), already rendered by the extractor —
    /// hence an opaque handle here. Null for `/SMask /None`.
    std::shared_ptr<const SoftMask> soft_mask;
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
    /// The `cs`/`CS`-selected non-device space (owned by `Resources`), through
    /// which `sc`/`scn` components are converted into `rgb` as the operator
    /// runs. Null for a device space; cleared by `g`/`rg`/`k`.
    const ColorSpaceDef *def{nullptr};
    /// The `/Pattern` resource name `scn`/`SCN` selected, resolved against
    /// `Resources::pattern` at paint time. Cleared by `g`/`rg`/`k`.
    std::string pattern;
  };

  struct State {
    General general;
    Text text;
    Color stroke_color;
    Color other_color;
    /// Clip in force: the intersection of these (empty = the whole page). Part
    /// of the saved state (ISO 32000-1 8.5.4), so `q`/`Q` scope it.
    std::vector<ClipPath> clip;
  };

  /// Never empty: `current()` is `back()`, and `restore()` keeps the base.
  std::vector<State> stack;

  /// The path under construction. *Not* part of the `q`/`Q` stack (ISO 32000-1
  /// 8.5.2); a path-painting operator consumes and clears it.
  std::vector<Subpath> path;

  /// Call-chain depths bounding self-referential Type3 char procs and soft-mask
  /// groups. Deliberately outside `State`: they are not graphics state, so
  /// `q`/`Q` must not scope them.
  std::int32_t type3_depth{0};
  std::int32_t soft_mask_depth{0};

  GraphicsState();

  State &current();
  [[nodiscard]] const State &current() const;

  void execute(const GraphicsOperator &);

  /// Path construction. Operands arrive in the operator's coordinate space and
  /// are stored mapped through the current CTM, i.e. in user space.
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
  /// Drop the accumulated path, as every path-painting operator does.
  void clear_path();

  /// Clipping (`W`/`W*`, ISO 32000-1 8.5.4). `set_pending_clip` only records
  /// the intent; the next painting (or `n`) operator calls `commit_clip` to
  /// intersect the path into the clip. That operator's own paint is still under
  /// the *old* clip, so callers snapshot the clip before committing.
  void set_pending_clip(bool even_odd);
  void commit_clip();
  /// Intersect a rectangle given in the current CTM's space (e.g. a form's
  /// `/BBox`) into the clip.
  void clip_bounding_box(double x0, double y0, double x1, double y1);

  /// `q`: push a copy of the current state.
  void save();
  /// `Q`: pop it. An unmatched `Q` is ignored — the base state always remains.
  void restore();
  /// `CTM = matrix * CTM`, as `cm` and form invocation do.
  void concat_matrix(const util::math::Transform2D &matrix);

  /// Text space -> user space with horizontal scaling and rise folded in, but
  /// *not* the font size: that is applied separately as the rendered
  /// font-size, which leaves the run-vs-glyph mapping to the renderer.
  [[nodiscard]] util::math::Transform2D text_placement_transform() const;

  /// Advance `Tm` by `(tx, ty)` in text space after showing glyphs; `Tlm` is
  /// unaffected.
  void advance_text(double tx, double ty);

private:
  /// `Tlm = translate(tx, ty) * Tlm`, `Tm = Tlm` — behind
  /// `Td`/`TD`/`T*`/`'`/`"`.
  void next_line(double tx, double ty);

  [[nodiscard]] std::array<double, 2> to_user(double x, double y) const;
  /// Append `segment`, opening a subpath at the current point if a construction
  /// operator other than `m`/`re` started the path (lenient).
  void append_segment(const PathSegment &segment);

  std::array<double, 2> m_current_point{0, 0}; // user space
  std::array<double, 2> m_subpath_start{0, 0}; // user space, for `h`/close

  /// A pending `W`/`W*`. Not part of the saved state: a `W` is always followed
  /// by a painting/`n` operator before any `q`/`Q` (ISO 32000-1 8.5.4).
  enum class PendingClip { none, nonzero, even_odd };
  PendingClip m_pending_clip{PendingClip::none};
};

} // namespace odr::internal::pdf
