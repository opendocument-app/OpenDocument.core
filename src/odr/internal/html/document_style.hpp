#pragma once

#include <optional>
#include <string>

namespace odr {
enum class TextAlign;
enum class HorizontalAlign;
enum class VerticalAlign;
enum class FontWeight;
enum class FontStyle;
enum class FontPosition;
enum class BreakType;

class Frame;
class Rect;
class Circle;
class CustomShape;

struct TextStyle;
struct ParagraphStyle;
struct TableStyle;
struct TableColumnStyle;
struct TableRowStyle;
struct TableCellStyle;
struct GraphicStyle;
struct DrawingTransform;
struct PageLayout;
} // namespace odr

namespace odr::internal::html {

/// `nullptr` for a break turned off: the css default already says it.
const char *translate_break(BreakType break_type);
const char *translate_text_align(TextAlign text_align);
const char *translate_horizontal_align(HorizontalAlign horizontal_align);
const char *translate_vertical_align(VerticalAlign vertical_align);
const char *translate_font_weight(FontWeight font_weight);
const char *translate_font_style(FontStyle font_style);
const char *translate_font_position(FontPosition font_position);

std::string translate_outer_page_style(const PageLayout &page_layout);
std::string translate_outer_flowing_page_style(const PageLayout &page_layout);
std::string translate_inner_page_style(const PageLayout &page_layout);
std::string translate_text_style(const TextStyle &text_style);
/// The part of a text style a block may carry: what paints belongs on the run.
std::string translate_block_font_style(const TextStyle &text_style);
std::string translate_paragraph_style(const ParagraphStyle &paragraph_style);
std::string translate_table_style(const TableStyle &table_style);
std::string
translate_table_column_style(const TableColumnStyle &table_column_style);
std::string translate_table_row_style(const TableRowStyle &table_row_style);
std::string translate_table_cell_style(const TableCellStyle &table_cell_style);
std::string translate_drawing_style(const GraphicStyle &graphic_style);
/// Empty for a transform that moves nothing.
std::string
translate_drawing_transform(const std::optional<DrawingTransform> &transform);

std::string translate_frame_properties(const Frame &frame);
std::string translate_rect_properties(const Rect &rect);
std::string translate_circle_properties(const Circle &circle);
std::string translate_custom_shape_properties(const CustomShape &custom_shape);

} // namespace odr::internal::html
