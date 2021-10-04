#include <odr/style.h>

namespace odr {

Color::Color() = default;

Color::Color(const std::uint32_t rgb)
    : red{(std::uint8_t)(rgb >> 16)}, green{(std::uint8_t)(rgb >> 8)},
      blue{(std::uint8_t)(rgb >> 0)} {}

Color::Color(const std::uint32_t rgba, bool)
    : red{(std::uint8_t)(rgba >> 24)}, green{(std::uint8_t)(rgba >> 16)},
      blue{(std::uint8_t)(rgba >> 8)}, alpha{(std::uint8_t)(rgba >> 0)} {}

Color::Color(const std::uint8_t red, const std::uint8_t green,
             const std::uint8_t blue)
    : red{red}, green{green}, blue{blue} {}

Color::Color(const std::uint8_t red, const std::uint8_t green,
             const std::uint8_t blue, const std::uint8_t alpha)
    : red{red}, green{green}, blue{blue}, alpha{alpha} {}

std::uint32_t Color::rgb() const {
  std::uint32_t result{0};
  result |= red << 16;
  result |= green << 8;
  result |= blue << 0;
  return result;
}

std::uint32_t Color::rgba() const {
  std::uint32_t result{0};
  result |= red << 24;
  result |= green << 16;
  result |= blue << 8;
  result |= alpha << 0;
  return result;
}

void TextStyle::override(const TextStyle &other) {
  if (other.font_name) {
    font_name = other.font_name;
  }
  if (other.font_size) {
    font_size = other.font_size;
  }
  if (other.font_weight) {
    font_weight = other.font_weight;
  }
  if (other.font_style) {
    font_style = other.font_style;
  }
  if (other.font_underline) {
    font_underline = other.font_underline;
  }
  if (other.font_line_through) {
    font_line_through = other.font_line_through;
  }
  if (other.font_shadow) {
    font_shadow = other.font_shadow;
  }
  if (other.font_color) {
    font_color = other.font_color;
  }
  if (other.background_color) {
    background_color = other.background_color;
  }
}

void ParagraphStyle::override(const ParagraphStyle &other) {
  if (other.text_align) {
    text_align = other.text_align;
  }
  margin.override(other.margin);
  if (other.line_height) {
    line_height = other.line_height;
  }
}

void TableStyle::override(const TableStyle &other) {
  if (other.width) {
    width = other.width;
  }
}

void TableColumnStyle::override(const TableColumnStyle &other) {
  if (other.width) {
    width = other.width;
  }
}

void TableRowStyle::override(const TableRowStyle &other) {
  if (other.height) {
    height = other.height;
  }
}

void TableCellStyle::override(const TableCellStyle &other) {
  if (other.vertical_align) {
    vertical_align = other.vertical_align;
  }
  if (other.background_color) {
    background_color = other.background_color;
  }
  padding.override(other.padding);
  border.override(other.border);
}

void TableCellStyle::override(TableCellStyle &&other) {
  if (other.vertical_align) {
    vertical_align = other.vertical_align;
  }
  if (other.background_color) {
    background_color = std::move(other.background_color);
  }
  padding.override(std::move(other.padding));
  border.override(std::move(other.border));
}

void GraphicStyle::override(const GraphicStyle &other) {
  if (other.stroke_width) {
    stroke_width = other.stroke_width;
  }
  if (other.stroke_color) {
    stroke_color = other.stroke_color;
  }
  if (other.fill_color) {
    fill_color = other.fill_color;
  }
  if (other.vertical_align) {
    vertical_align = other.vertical_align;
  }
  if (other.text_wrap) {
    text_wrap = other.text_wrap;
  }
}

} // namespace odr
