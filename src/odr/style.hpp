#pragma once

#include <odr/quantity.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace odr {

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

/// @brief Collection of vertical font positions (sub/superscript).
enum class FontPosition {
  normal,
  super,
  sub,
};

/// @brief Collection of text alignments.
///
/// @ref TextAlign::start and @ref TextAlign::end name the edge @ref
/// TextDirection decides, as css reads them; the rest name an absolute side.
enum class TextAlign {
  left,
  right,
  center,
  justify,
  start,
  end,
};

/// @brief Collection of base directions a line of text runs in.
///
/// Vertical writing modes have no value here.
enum class TextDirection {
  left_to_right,
  right_to_left,
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

/// @brief Collection of manual breaks, as a document sets them; not the
/// automatic ones a layout engine computes.
///
/// @ref BreakType::none is a break turned explicitly off, which an unset
/// @ref ParagraphStyle::break_before is not.
enum class BreakType {
  none,
  page,
  column,
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

  /// @brief Builds an opaque color from a packed `0xRRGGBB` value.
  static Color from_rgb(std::uint32_t rgb);
  /// @brief Builds a color from a packed `0xAARRGGBB` value.
  static Color from_argb(std::uint32_t argb);

  Color();
  Color(std::uint8_t red, std::uint8_t green, std::uint8_t blue);
  Color(std::uint8_t red, std::uint8_t green, std::uint8_t blue,
        std::uint8_t alpha);

  [[nodiscard]] std::uint32_t rgb() const;
  [[nodiscard]] std::uint32_t argb() const;
};

inline Color operator""_rgb(const unsigned long long rgb) {
  return Color::from_rgb(static_cast<std::uint32_t>(rgb));
}

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
    if (other.right.has_value()) {
      right = other.right;
    }
    if (other.top.has_value()) {
      top = other.top;
    }
    if (other.left.has_value()) {
      left = other.left;
    }
    if (other.bottom.has_value()) {
      bottom = other.bottom;
    }
  }

  void override(DirectionalStyle &&other) {
    if (other.right.has_value()) {
      right = std::move(other.right);
    }
    if (other.top.has_value()) {
      top = std::move(other.top);
    }
    if (other.left.has_value()) {
      left = std::move(other.left);
    }
    if (other.bottom.has_value()) {
      bottom = std::move(other.bottom);
    }
  }
};

/// @brief Represents a style for text.
///
/// @note `font_name` borrows from the document that produced the style and is
/// only valid for as long as that document is alive.
struct TextStyle final {
  std::optional<std::string_view> font_name;
  std::optional<Measure> font_size;
  std::optional<FontWeight> font_weight;
  std::optional<FontStyle> font_style;
  std::optional<bool> font_underline;
  std::optional<bool> font_line_through;
  std::optional<std::string> font_shadow;
  std::optional<Color> font_color;
  std::optional<Color> background_color;
  std::optional<FontPosition> font_position;

  void override(const TextStyle &other);
};

/// @brief Represents a style for paragraphs.
struct ParagraphStyle final {
  std::optional<TextAlign> text_align;
  /// The base direction the paragraph's text runs in.
  std::optional<TextDirection> direction;
  DirectionalStyle<Measure> margin;
  std::optional<Measure> line_height;
  std::optional<Measure> text_indent;
  /// A break the author put before or after the paragraph.
  std::optional<BreakType> break_before;
  std::optional<BreakType> break_after;

  void override(const ParagraphStyle &other);
};

/// @brief Represents a style for tables.
struct TableStyle final {
  std::optional<Measure> width;
  /// The frame around the table, and the rules between its rows and columns.
  /// @note What the table states; the cells are what draws it, through
  /// @ref TableCellStyle::border.
  DirectionalStyle<std::string> border;
  std::optional<std::string> border_inside_horizontal;
  std::optional<std::string> border_inside_vertical;

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
  /// The side a frame sits on; unset where its offset decides instead.
  std::optional<HorizontalAlign> horizontal_position;

  void override(const GraphicStyle &other);
};

/// @brief Represents a layout for a page.
struct PageLayout final {
  std::optional<Measure> width;
  std::optional<Measure> height;
  std::optional<PrintOrientation> print_orientation;
  DirectionalStyle<Measure> margin;
  /// The ground the page is painted on; unset leaves it to the viewer.
  std::optional<Color> background_color;
  /// The base direction, inherited by paragraphs stating none of their own.
  std::optional<TextDirection> direction;
};

} // namespace odr
