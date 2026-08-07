#include <odr/style.hpp>

namespace odr {

namespace {

template <typename T>
void override_if_set(std::optional<T> &property,
                     const std::optional<T> &other) {
  if (other.has_value()) {
    property = other;
  }
}

} // namespace

Color Color::from_rgb(const std::uint32_t rgb) {
  return {static_cast<std::uint8_t>(rgb >> 16),
          static_cast<std::uint8_t>(rgb >> 8),
          static_cast<std::uint8_t>(rgb >> 0)};
}

Color Color::from_argb(const std::uint32_t argb) {
  return {static_cast<std::uint8_t>(argb >> 16),
          static_cast<std::uint8_t>(argb >> 8),
          static_cast<std::uint8_t>(argb >> 0),
          static_cast<std::uint8_t>(argb >> 24)};
}

Color::Color() = default;

Color::Color(const std::uint8_t red, const std::uint8_t green,
             const std::uint8_t blue)
    : red{red}, green{green}, blue{blue} {}

Color::Color(const std::uint8_t red, const std::uint8_t green,
             const std::uint8_t blue, const std::uint8_t alpha)
    : red{red}, green{green}, blue{blue}, alpha{alpha} {}

std::uint32_t Color::rgb() const {
  return static_cast<std::uint32_t>(red) << 16 |
         static_cast<std::uint32_t>(green) << 8 |
         static_cast<std::uint32_t>(blue);
}

std::uint32_t Color::argb() const {
  // `alpha` promotes to `int`, so it has to be widened before the shift
  return static_cast<std::uint32_t>(alpha) << 24 | rgb();
}

void TextStyle::override(const TextStyle &other) {
  override_if_set(font_name, other.font_name);
  override_if_set(font_size, other.font_size);
  override_if_set(font_weight, other.font_weight);
  override_if_set(font_style, other.font_style);
  override_if_set(font_underline, other.font_underline);
  override_if_set(font_line_through, other.font_line_through);
  override_if_set(font_shadow, other.font_shadow);
  override_if_set(font_color, other.font_color);
  override_if_set(background_color, other.background_color);
  override_if_set(font_position, other.font_position);
}

void ParagraphStyle::override(const ParagraphStyle &other) {
  override_if_set(text_align, other.text_align);
  margin.override(other.margin);
  override_if_set(line_height, other.line_height);
  override_if_set(text_indent, other.text_indent);
}

void TableStyle::override(const TableStyle &other) {
  override_if_set(width, other.width);
}

void TableColumnStyle::override(const TableColumnStyle &other) {
  override_if_set(width, other.width);
}

void TableRowStyle::override(const TableRowStyle &other) {
  override_if_set(height, other.height);
}

void TableCellStyle::override(const TableCellStyle &other) {
  override_if_set(horizontal_align, other.horizontal_align);
  override_if_set(vertical_align, other.vertical_align);
  override_if_set(background_color, other.background_color);
  padding.override(other.padding);
  border.override(other.border);
  override_if_set(text_rotation, other.text_rotation);
}

void GraphicStyle::override(const GraphicStyle &other) {
  override_if_set(stroke_width, other.stroke_width);
  override_if_set(stroke_color, other.stroke_color);
  override_if_set(fill_color, other.fill_color);
  override_if_set(vertical_align, other.vertical_align);
  override_if_set(text_wrap, other.text_wrap);
}

} // namespace odr
