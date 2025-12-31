#include <odr/internal/html/document_style.hpp>

#include <odr/document_element.hpp>
#include <odr/style.hpp>

#include <odr/internal/html/common.hpp>

namespace odr::internal {

const char *html::translate_text_align(const TextAlign text_align) {
  switch (text_align) {
  case TextAlign::left:
    return "left";
  case TextAlign::right:
    return "right";
  case TextAlign::center:
    return "center";
  case TextAlign::justify:
    return "justify";
  default:
    return ""; // TODO log
  }
}

const char *
html::translate_horizontal_align(const HorizontalAlign horizontal_align) {
  switch (horizontal_align) {
  case HorizontalAlign::left:
    return "left";
  case HorizontalAlign::center:
    return "center";
  case HorizontalAlign::right:
    return "right";
  default:
    return ""; // TODO log
  }
}

const char *html::translate_vertical_align(const VerticalAlign vertical_align) {
  switch (vertical_align) {
  case VerticalAlign::top:
    return "top";
  case VerticalAlign::middle:
    return "middle";
  case VerticalAlign::bottom:
    return "bottom";
  default:
    return ""; // TODO log
  }
}

const char *html::translate_font_weight(const FontWeight font_weight) {
  switch (font_weight) {
  case FontWeight::normal:
    return "normal";
  case FontWeight::bold:
    return "bold";
  default:
    return ""; // TODO log
  }
}

const char *html::translate_font_style(const FontStyle font_style) {
  switch (font_style) {
  case FontStyle::normal:
    return "normal";
  case FontStyle::italic:
    return "italic";
  default:
    return ""; // TODO log
  }
}

std::string html::translate_outer_page_style(const PageLayout &page_layout) {
  std::string result;
  if (const std::optional<Measure> width = page_layout.width;
      width.has_value()) {
    result.append("width:").append(width->to_string()).append(";");
  }
  if (const std::optional<Measure> height = page_layout.height;
      height.has_value()) {
    result.append("height:").append(height->to_string()).append(";");
  }
  return result;
}

std::string html::translate_inner_page_style(const PageLayout &page_layout) {
  std::string result;
  if (const std::optional<Quantity<double>> margin_right =
          page_layout.margin.right;
      margin_right.has_value()) {
    result.append("margin-right:")
        .append(margin_right->to_string())
        .append(";");
  }
  if (const std::optional<Quantity<double>> margin_top = page_layout.margin.top;
      margin_top.has_value()) {
    result.append("margin-top:").append(margin_top->to_string()).append(";");
  }
  if (const std::optional<Quantity<double>> margin_left =
          page_layout.margin.left;
      margin_left.has_value()) {
    result.append("margin-left:").append(margin_left->to_string()).append(";");
  }
  if (const std::optional<Quantity<double>> margin_bottom =
          page_layout.margin.bottom;
      margin_bottom.has_value()) {
    result.append("margin-bottom:")
        .append(margin_bottom->to_string())
        .append(";");
  }
  return result;
}

std::string html::translate_text_style(const TextStyle &text_style) {
  std::string result;
  if (const char *font_name = text_style.font_name; font_name != nullptr) {
    result.append("font-family:").append(font_name).append(";");
  }
  if (const std::optional<Measure> font_size = text_style.font_size;
      font_size.has_value()) {
    result.append("font-size:").append(font_size->to_string()).append(";");
  }
  if (const std::optional<FontWeight> font_weight = text_style.font_weight;
      font_weight.has_value()) {
    result.append("font-weight:")
        .append(translate_font_weight(*font_weight))
        .append(";");
  }
  if (const std::optional<FontStyle> font_style = text_style.font_style;
      font_style.has_value()) {
    result.append("font-style:")
        .append(translate_font_style(*font_style))
        .append(";");
  }
  if (text_style.font_underline && *text_style.font_underline) {
    result += "text-decoration:underline;";
  }
  if (text_style.font_line_through && *text_style.font_line_through) {
    result += "text-decoration:line-through;";
  }
  if (const std::optional<std::string> font_shadow = text_style.font_shadow;
      font_shadow.has_value()) {
    result.append("text-shadow:").append(*font_shadow).append(";");
  }
  if (const std::optional<Color> font_color = text_style.font_color;
      font_color.has_value()) {
    result.append("color:").append(color(*font_color)).append(";");
  }
  if (const std::optional<Color> background_color = text_style.background_color;
      background_color.has_value()) {
    result.append("background-color:")
        .append(color(*background_color))
        .append(";");
  }
  return result;
}

std::string
html::translate_paragraph_style(const ParagraphStyle &paragraph_style) {
  std::string result;
  if (const std::optional<TextAlign> text_align = paragraph_style.text_align;
      text_align.has_value()) {
    result.append("text-align:")
        .append(translate_text_align(*text_align))
        .append(";");
  }
  if (const std::optional<Quantity<double>> margin_right =
          paragraph_style.margin.right;
      margin_right.has_value()) {
    result.append("margin-right:")
        .append(margin_right->to_string())
        .append(";");
  }
  if (const std::optional<Quantity<double>> margin_top =
          paragraph_style.margin.top;
      margin_top.has_value()) {
    result.append("margin-top:").append(margin_top->to_string()).append(";");
  }
  if (const std::optional<Quantity<double>> margin_left =
          paragraph_style.margin.left;
      margin_left.has_value()) {
    result.append("margin-left:").append(margin_left->to_string()).append(";");
  }
  if (const std::optional<Quantity<double>> margin_bottom =
          paragraph_style.margin.bottom;
      margin_bottom.has_value()) {
    result.append("margin-bottom:")
        .append(margin_bottom->to_string())
        .append(";");
  }
  if (const std::optional<Measure> line_height = paragraph_style.line_height;
      line_height.has_value()) {
    result.append("line-height:").append(line_height->to_string()).append(";");
  }
  return result;
}

std::string html::translate_table_style(const TableStyle &table_style) {
  std::string result;
  if (const std::optional<Measure> width = table_style.width;
      width.has_value()) {
    result.append("width:").append(width->to_string()).append(";");
  }
  return result;
}

std::string
html::translate_table_column_style(const TableColumnStyle &table_column_style) {
  std::string result;
  if (const std::optional<Measure> width = table_column_style.width;
      width.has_value()) {
    result.append("width:").append(width->to_string()).append(";");
    result.append("min-width:").append(width->to_string()).append(";");
  }
  return result;
}

std::string
html::translate_table_row_style(const TableRowStyle &table_row_style) {
  std::string result;
  // TODO that does not work with HTML; height would need to be applied to the
  // cells
  if (const std::optional<Measure> height = table_row_style.height;
      height.has_value()) {
    result.append("height:").append(height->to_string()).append(";");
  }
  return result;
}

std::string
html::translate_table_cell_style(const TableCellStyle &table_cell_style) {
  std::string result;
  if (const std::optional<HorizontalAlign> horizontal_align =
          table_cell_style.horizontal_align;
      horizontal_align.has_value()) {
    result.append("text-align:")
        .append(translate_horizontal_align(*horizontal_align))
        .append(";");
  }
  if (const std::optional<VerticalAlign> vertical_align =
          table_cell_style.vertical_align;
      vertical_align.has_value()) {
    result.append("vertical-align:")
        .append(translate_vertical_align(*vertical_align))
        .append(";");
  }
  if (const std::optional<Color> background_color =
          table_cell_style.background_color;
      background_color.has_value()) {
    result.append("background-color:")
        .append(color(*background_color))
        .append(";");
  }
  if (const std::optional<Quantity<double>> padding_right =
          table_cell_style.padding.right;
      padding_right.has_value()) {
    result.append("padding-right:")
        .append(padding_right->to_string())
        .append(";");
  }
  if (const std::optional<Quantity<double>> padding_top =
          table_cell_style.padding.top;
      padding_top.has_value()) {
    result.append("padding-top:").append(padding_top->to_string()).append(";");
  }
  if (const std::optional<Quantity<double>> padding_left =
          table_cell_style.padding.left;
      padding_left.has_value()) {
    result.append("padding-left:")
        .append(padding_left->to_string())
        .append(";");
  }
  if (const std::optional<Quantity<double>> padding_bottom =
          table_cell_style.padding.bottom;
      padding_bottom.has_value()) {
    result.append("padding-bottom:")
        .append(padding_bottom->to_string())
        .append(";");
  }
  if (const std::optional<std::string> border_right =
          table_cell_style.border.right;
      border_right.has_value()) {
    result.append("border-right:").append(*border_right).append(";");
  }
  if (const std::optional<std::string> border_top = table_cell_style.border.top;
      border_top.has_value()) {
    result.append("border-top:").append(*border_top).append(";");
  }
  if (const std::optional<std::string> border_left =
          table_cell_style.border.left;
      border_left.has_value()) {
    result.append("border-left:").append(*border_left).append(";");
  }
  if (const std::optional<std::string> border_bottom =
          table_cell_style.border.bottom;
      border_bottom.has_value()) {
    result.append("border-bottom:").append(*border_bottom).append(";");
  }
  if (const std::optional<double> text_rotation =
          table_cell_style.text_rotation;
      text_rotation.has_value() && *text_rotation != 0) {
    // TODO covers only the -90° case
    result.append("writing-mode:vertical-lr;");
  }
  return result;
}

std::string html::translate_drawing_style(const GraphicStyle &graphic_style) {
  std::string result;
  if (const std::optional<Measure> stroke_width = graphic_style.stroke_width;
      stroke_width.has_value()) {
    result.append("stroke-width:")
        .append(stroke_width->to_string())
        .append(";");
  }
  if (const std::optional<Color> stroke_color = graphic_style.stroke_color;
      stroke_color.has_value()) {
    result.append("stroke:").append(color(*stroke_color)).append(";");
  }
  if (const std::optional<Color> fill_color = graphic_style.fill_color;
      fill_color.has_value()) {
    result.append("fill:").append(color(*fill_color)).append(";");
  }
  if (const std::optional<VerticalAlign> vertical_align =
          graphic_style.vertical_align;
      vertical_align.has_value()) {
    if (vertical_align == VerticalAlign::middle) {
      result += "display:flex;justify-content:center;flex-direction:column;";
    }
    // TODO else log
  }
  return result;
}

std::string html::translate_frame_properties(const Frame &frame) {
  auto text_wrap = TextWrap::run_through;
  if (const GraphicStyle style = frame.style(); style.text_wrap.has_value()) {
    text_wrap = *style.text_wrap;
  }

  std::string result;
  if (const AnchorType anchor_type = frame.anchor_type();
      anchor_type == AnchorType::as_char) {
    result += "display:inline-block;";
  } else if (text_wrap == TextWrap::before) {
    result += "display:block;";
    result += "float:right;clear:both;";
    result += "shape-outside:content-box;";
    if (const std::optional<std::string> x = frame.x(); x.has_value()) {
      result += "margin-left:" + *x + ";";
    }
    if (const std::optional<std::string> y = frame.y(); y.has_value()) {
      result += "margin-top:" + *y + ";";
    }
    result += "margin-right:calc(100% - ";
    result += frame.x().value_or("0in");
    result += " - ";
    result += *frame.width();
    result += ");";
  } else if (text_wrap == TextWrap::after) {
    result += "display:block;";
    result += "float:left;clear:both;";
    result += "shape-outside:content-box;";
    if (const std::optional<std::string> x = frame.x(); x.has_value()) {
      result += "margin-left:" + *x + ";";
    }
    if (const std::optional<std::string> y = frame.y(); y.has_value()) {
      result += "margin-top:" + *y + ";";
    }
  } else if (text_wrap == TextWrap::none) {
    result += "display:block;";
    if (const std::optional<std::string> x = frame.x(); x.has_value()) {
      result += "margin-left:" + *x + ";";
    }
    if (const std::optional<std::string> y = frame.y(); y.has_value()) {
      result += "margin-top:" + *y + ";";
    }
  } else {
    result += "display:block;";
    result += "position:absolute;";
    if (const std::optional<std::string> x = frame.x(); x.has_value()) {
      result += "left:" + *x + ";";
    }
    if (const std::optional<std::string> y = frame.y(); y.has_value()) {
      result += "top:" + *y + ";";
    }
  }
  if (const std::optional<std::string> width = frame.width();
      width.has_value()) {
    result += "width:" + *width + ";";
  }
  if (const std::optional<std::string> height = frame.height();
      height.has_value()) {
    result += "height:" + *height + ";";
  }
  if (const std::optional<std::string> z_index = frame.z_index();
      z_index.has_value()) {
    result += "z-index:" + *z_index + ";";
  }
  return result;
}

std::string html::translate_rect_properties(const Rect &rect) {
  std::string result;
  result += "position:absolute;";
  result += "left:" + rect.x() + ";";
  result += "top:" + rect.y() + ";";
  result += "width:" + rect.width() + ";";
  result += "height:" + rect.height() + ";";
  return result;
}

std::string html::translate_circle_properties(const Circle &circle) {
  std::string result;
  result += "position:absolute;";
  result += "left:" + circle.x() + ";";
  result += "top:" + circle.y() + ";";
  result += "width:" + circle.width() + ";";
  result += "height:" + circle.height() + ";";
  return result;
}

std::string
html::translate_custom_shape_properties(const CustomShape &custom_shape) {
  std::string result;
  result += "position:absolute;";
  if (const std::optional<std::string> x = custom_shape.x(); x.has_value()) {
    result += "left:" + *x + ";";
  } else {
    result += "left:0;";
  }
  if (const std::optional<std::string> y = custom_shape.y(); y.has_value()) {
    result += "top:" + *y + ";";
  } else {
    result += "top:0;";
  }
  result += "width:" + custom_shape.width() + ";";
  result += "height:" + custom_shape.height() + ";";
  return result;
}

} // namespace odr::internal
