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

  void override(DirectionalStyle &&other) {
    if (other.right) {
      right = std::move(other.right);
    }
    if (other.top) {
      top = std::move(other.top);
    }
    if (other.left) {
      left = std::move(other.left);
    }
    if (other.bottom) {
      bottom = std::move(other.bottom);
    }
  }
};

struct TextStyle final {
  std::optional<std::string> font_name;
  std::optional<std::string> font_size;
  std::optional<std::string> font_weight;
  std::optional<std::string> font_style;
  std::optional<std::string> font_underline;
  std::optional<std::string> font_line_through;
  std::optional<std::string> font_shadow;
  std::optional<std::string> font_color;
  std::optional<std::string> background_color;

  void override(const TextStyle &other);
  void override(TextStyle &&other);
};

struct ParagraphStyle final {
  std::optional<TextAlign> text_align;
  DirectionalStyle<std::string> margin;

  void override(const ParagraphStyle &other);
  void override(ParagraphStyle &&other);
};

struct TableStyle final {
  std::optional<std::string> width;

  void override(const TableStyle &other);
  void override(TableStyle &&other);
};

struct TableColumnStyle final {
  std::optional<std::string> width;

  void override(const TableColumnStyle &other);
  void override(TableColumnStyle &&other);
};

struct TableRowStyle final {
  std::optional<std::string> height;

  void override(const TableRowStyle &other);
  void override(TableRowStyle &&other);
};

struct TableCellStyle final {
  std::optional<VerticalAlign> vertical_align;
  std::optional<std::string> background_color;
  DirectionalStyle<std::string> padding;
  DirectionalStyle<std::string> border;

  void override(const TableCellStyle &other);
  void override(TableCellStyle &&other);
};

struct GraphicStyle final {
  std::optional<std::string> stroke_width;
  std::optional<std::string> stroke_color;
  std::optional<std::string> fill_color;
  std::optional<VerticalAlign> vertical_align;

  void override(const GraphicStyle &other);
  void override(GraphicStyle &&other);
};

struct PageLayout final {
  std::optional<std::string> width;
  std::optional<std::string> height;
  std::optional<std::string> print_orientation;
  DirectionalStyle<std::string> margin;
};

struct TableDimensions {
  std::uint32_t rows{0};
  std::uint32_t columns{0};

  TableDimensions();
  TableDimensions(std::uint32_t rows, std::uint32_t columns);
};

} // namespace odr

#endif // ODR_STYLE_H
