#pragma once

#include <odr/internal/pdf/pdf_geometry.hpp>

#include <array>
#include <string>
#include <vector>

namespace odr::internal::pdf {

struct GraphicsOperator;

enum class ColorSpace {
  unknown,
  device_grey,
  device_rgb,
  device_cmyk,
};

struct GraphicsState {
  struct General {
    double line_width{};
    int cap_style{};
    int join_style{};
    double miter_limit{};
    int dash_pattern{};
    std::string color_rendering_intent;
    double flatness_tolerance{};
    std::string graphics_state_parameters;
    Matrix transform_matrix; // CTM
  };

  struct Path {
    std::array<double, 2> current_position{0, 0};
    // TODO clipping
  };

  struct Text {
    double char_spacing{0};         // Tc
    double word_spacing{0};         // Tw
    double horizontal_scaling{100}; // Tz, in percent (100 = normal)
    double leading{0};              // TL
    std::string font;               // Tf resource name
    double size{};                  // Tf size
    int rendering_mode{0};          // Tr
    double rise{0};                 // Ts
    Matrix matrix;                  // Tm
    Matrix line_matrix;             // Tlm
    std::array<double, 2> glyph_width{};
    std::array<double, 4> glyph_bounding_box{};
  };

  struct Color {
    ColorSpace space{ColorSpace::device_grey};
    // TODO "color"
    double grey{};
    std::array<double, 3> rgb{};
    std::array<double, 4> cmyk{};
  };

  struct State {
    General general;
    Path path;
    Text text;
    Color stroke_color;
    Color other_color;
  };

  std::vector<State> stack;

  GraphicsState();

  State &current();
  [[nodiscard]] const State &current() const;

  void execute(const GraphicsOperator &);

  /// Text rendering matrix *excluding* the font size: maps text space (1 unit =
  /// 1 em at the current font size) to user space, with horizontal scaling and
  /// rise folded in. The font size is applied separately (as the rendered
  /// font-size), which keeps the run-vs-glyph mapping decision in the renderer.
  [[nodiscard]] Matrix text_placement_matrix() const;

private:
  /// Move to the start of a new text line: `Tlm = translate(tx, ty) * Tlm` and
  /// `Tm = Tlm` (the shared mechanic behind `Td`, `TD`, `T*`, `'`, `"`).
  void next_line(double tx, double ty);
};

} // namespace odr::internal::pdf
