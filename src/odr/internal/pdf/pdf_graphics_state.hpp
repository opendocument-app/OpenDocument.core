#pragma once

#include <array>
#include <cstdint>
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
    double color_rendering_intent{};
    double flatness_tolerance{};
    std::string graphics_state_parameters;
    std::array<double, 6> transform_matrix{1, 0, 0, 1, 0, 0};
  };

  struct Path {
    std::array<double, 2> current_position{0, 0};
    // TODO clipping
  };

  struct Text {
    double char_spacing{0};
    double word_spacing{0};
    double horizontal_scaling{1};
    double leading{0};
    std::string font;
    double size{};
    int rendering_mode{0};
    double rise{0};
    std::array<double, 2> offset{0, 0};
    std::array<double, 6> transform_matrix{1, 0, 0, 1, 0, 0};
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
  const State &current() const;

  void execute(const GraphicsOperator &);
};

} // namespace odr::internal::pdf
