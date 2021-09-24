#ifndef ODR_STYLE_H
#define ODR_STYLE_H

#include <cstdint>
#include <odr/quantity.h>
#include <optional>
#include <string>
#include <utility>

namespace odr {

using Measure = Quantity<double>;

enum class FontWeight {
  normal,
  bold,
};

enum class FontStyle {
  normal,
  italic,
};

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

enum class PrintOrientation {
  portrait,
  landscape,
};

struct Color final {
  std::uint8_t red{0};
  std::uint8_t green{0};
  std::uint8_t blue{0};
  std::uint8_t alpha{255};

  Color();
  explicit Color(std::uint32_t rgb);
  Color(std::uint32_t rgba, bool dummy);
  Color(std::uint8_t red, std::uint8_t green, std::uint8_t blue);
  Color(std::uint8_t red, std::uint8_t green, std::uint8_t blue,
        std::uint8_t alpha);

  [[nodiscard]] std::uint32_t rgb() const;
  [[nodiscard]] std::uint32_t rgba() const;
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
  const char *font_name{nullptr};
  std::optional<Measure> font_size;
  std::optional<FontWeight> font_weight;
  std::optional<FontStyle> font_style;
  bool font_underline{false};
  bool font_line_through{false};
  std::optional<std::string> font_shadow;
  std::optional<Color> font_color;
  std::optional<Color> background_color;

  void override(const TextStyle &other);
};

struct ParagraphStyle final {
  std::optional<TextAlign> text_align;
  DirectionalStyle<Measure> margin;

  void override(const ParagraphStyle &other);
};

struct TableStyle final {
  std::optional<Measure> width;

  void override(const TableStyle &other);
};

struct TableColumnStyle final {
  std::optional<Measure> width;

  void override(const TableColumnStyle &other);
};

struct TableRowStyle final {
  std::optional<Measure> height;

  void override(const TableRowStyle &other);
};

struct TableCellStyle final {
  std::optional<VerticalAlign> vertical_align;
  std::optional<Color> background_color;
  DirectionalStyle<Measure> padding;
  DirectionalStyle<std::string> border;

  void override(const TableCellStyle &other);
  void override(TableCellStyle &&other);
};

struct GraphicStyle final {
  std::optional<Measure> stroke_width;
  std::optional<Color> stroke_color;
  std::optional<Color> fill_color;
  std::optional<VerticalAlign> vertical_align;
  bool text_wrap{false};

  void override(const GraphicStyle &other);
};

struct PageLayout final {
  std::optional<Measure> width;
  std::optional<Measure> height;
  std::optional<PrintOrientation> print_orientation;
  DirectionalStyle<Measure> margin;
};

struct TableDimensions {
  std::uint32_t rows{0};
  std::uint32_t columns{0};

  TableDimensions();
  TableDimensions(std::uint32_t rows, std::uint32_t columns);
};

} // namespace odr

#endif // ODR_STYLE_H
