#ifndef ODR_STYLE_H
#define ODR_STYLE_H

#include <optional>
#include <string>

namespace odr {

enum class TextAlign {
  left,
  right,
  center,
  justify,
};

enum class VerticalAlign {
  top,
  middle,
  bottom,
};

struct DirectionalStringStyle final {
  const char *right;
  const char *top;
  const char *left;
  const char *bottom;

  DirectionalStringStyle() = default;
  DirectionalStringStyle(const char *all)
      : right{all}, top{all}, left{all}, bottom{all} {}

  void override(const DirectionalStringStyle &other) {
    if (other.right) {
      right = other.right;
    }
    if (other.top) {
      top = other.top;
    }
    if (other.left) {
      left = other.left;
    }
    if (other.bottom) {
      bottom = other.bottom;
    }
  }
};

template <typename T> struct DirectionalStyle final {
  std::optional<T> right;
  std::optional<T> top;
  std::optional<T> left;
  std::optional<T> bottom;

  DirectionalStyle() = default;
  DirectionalStyle(std::optional<T> all)
      : right{all}, top{all}, left{all}, bottom{all} {}

  void override(const DirectionalStyle &other) {
    if (other.right) {
      right = other.right;
    }
    if (other.top) {
      top = other.top;
    }
    if (other.left) {
      left = other.left;
    }
    if (other.bottom) {
      bottom = other.bottom;
    }
  }
};

struct TextStyle final {
  const char *font_name;
  const char *font_size;
  const char *font_weight;
  const char *font_style;
  const char *font_underline;
  const char *font_line_through;
  const char *font_shadow;
  const char *font_color;
  const char *background_color;

  void override(const TextStyle &other);
};

struct ParagraphStyle final {
  std::optional<TextAlign> text_align;
  DirectionalStringStyle margin;

  void override(const ParagraphStyle &other);
};

struct TableStyle final {
  const char *width;

  void override(const TableStyle &other);
};

struct TableColumnStyle final {
  const char *width;

  void override(const TableColumnStyle &other);
};

struct TableRowStyle final {
  const char *height;

  void override(const TableRowStyle &other);
};

struct TableCellStyle final {
  std::optional<VerticalAlign> vertical_align;
  const char *background_color;
  DirectionalStringStyle padding;
  DirectionalStringStyle border;

  void override(const TableCellStyle &other);
};

struct GraphicStyle final {
  const char *stroke_width;
  const char *stroke_color;
  const char *fill_color;
  std::optional<VerticalAlign> vertical_align;

  void override(const GraphicStyle &other);
};

struct PageLayout final {
  const char *width;
  const char *height;
  const char *print_orientation;
  DirectionalStringStyle margin;
};

struct TableDimensions {
  std::uint32_t rows{0};
  std::uint32_t columns{0};

  TableDimensions();
  TableDimensions(std::uint32_t rows, std::uint32_t columns);
};

} // namespace odr

#endif // ODR_STYLE_H
