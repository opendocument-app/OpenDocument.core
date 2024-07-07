#ifndef ODR_STYLE_HPP
#define ODR_STYLE_HPP

#include <odr/quantity.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace odr {

/// @brief Represents a quantity: a magnitude and a unit of measure.
using Measure = Quantity<double>;

/// @brief Collection of font weights.
enum class FontWeight {
  normal,
  bold,
};

/// @brief Collection of font styles.
enum class FontStyle {
  normal,
  italic,
};

/// @brief Collection of text alignments.
enum class TextAlign {
  left,
  right,
  center,
  justify,
};

/// @brief Collection of horizontal alignments.
enum class HorizontalAlign {
  left,
  center,
  right,
};

/// @brief Collection of vertical alignments.
enum class VerticalAlign {
  top,
  middle,
  bottom,
};

/// @brief Collection of print orientations.
enum class PrintOrientation {
  portrait,
  landscape,
};

/// @brief Collection of text wrapping options.
enum class TextWrap {
  none,
  before,
  after,
  run_through,
};

/// @brief Represents a color.
struct Color final {
  std::uint8_t red{0};
  std::uint8_t green{0};
  std::uint8_t blue{0};
  std::uint8_t alpha{255};

  Color();
  Color(std::uint32_t rgb);
  Color(std::uint32_t argb, bool dummy);
  Color(std::uint8_t red, std::uint8_t green, std::uint8_t blue);
  Color(std::uint8_t red, std::uint8_t green, std::uint8_t blue,
        std::uint8_t alpha);

  [[nodiscard]] std::uint32_t rgb() const;
  [[nodiscard]] std::uint32_t argb() const;
};

/// @brief Represents a directional style.
template <typename T> struct DirectionalStyle final {
  std::optional<T> right;
  std::optional<T> top;
  std::optional<T> left;
  std::optional<T> bottom;

  DirectionalStyle() = default;
  explicit DirectionalStyle(std::optional<T> all)
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

/// @brief Represents a style for text.
struct TextStyle final {
  const char *font_name{nullptr};
  std::optional<Measure> font_size;
  std::optional<FontWeight> font_weight;
  std::optional<FontStyle> font_style;
  std::optional<bool> font_underline;
  std::optional<bool> font_line_through;
  std::optional<std::string> font_shadow;
  std::optional<Color> font_color;
  std::optional<Color> background_color;

  void override(const TextStyle &other);
};

/// @brief Represents a style for paragraphs.
struct ParagraphStyle final {
  std::optional<TextAlign> text_align;
  DirectionalStyle<Measure> margin;
  std::optional<Measure> line_height;

  void override(const ParagraphStyle &other);
};

/// @brief Represents a style for tables.
struct TableStyle final {
  std::optional<Measure> width;

  void override(const TableStyle &other);
};

/// @brief Represents a style for table columns.
struct TableColumnStyle final {
  std::optional<Measure> width;

  void override(const TableColumnStyle &other);
};

/// @brief Represents a style for table rows.
struct TableRowStyle final {
  std::optional<Measure> height;

  void override(const TableRowStyle &other);
};

/// @brief Represents a style for table cells.
struct TableCellStyle final {
  std::optional<HorizontalAlign> horizontal_align;
  std::optional<VerticalAlign> vertical_align;
  std::optional<Color> background_color;
  DirectionalStyle<Measure> padding;
  DirectionalStyle<std::string> border;
  std::optional<double> text_rotation;

  void override(const TableCellStyle &other);
};

/// @brief Represents a style for graphics.
struct GraphicStyle final {
  std::optional<Measure> stroke_width;
  std::optional<Color> stroke_color;
  std::optional<Color> fill_color;
  std::optional<VerticalAlign> vertical_align;
  std::optional<TextWrap> text_wrap;

  void override(const GraphicStyle &other);
};

/// @brief Represents a layout for a page.
struct PageLayout final {
  std::optional<Measure> width;
  std::optional<Measure> height;
  std::optional<PrintOrientation> print_orientation;
  DirectionalStyle<Measure> margin;
};

/// @brief Represents the dimensions of a table.
struct TableDimensions {
  std::uint32_t rows{0};
  std::uint32_t columns{0};

  TableDimensions();
  TableDimensions(std::uint32_t rows, std::uint32_t columns);
};

} // namespace odr

#endif // ODR_STYLE_HPP
