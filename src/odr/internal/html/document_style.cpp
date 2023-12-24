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
  if (auto width = page_layout.width) {
    result.append("width:").append(width->to_string()).append(";");
  }
  if (auto height = page_layout.height) {
    result.append("height:").append(height->to_string()).append(";");
  }
  return result;
}

std::string html::translate_inner_page_style(const PageLayout &page_layout) {
  std::string result;
  if (auto margin_right = page_layout.margin.right) {
    result.append("margin-right:")
        .append(margin_right->to_string())
        .append(";");
  }
  if (auto margin_top = page_layout.margin.top) {
    result.append("margin-top:").append(margin_top->to_string()).append(";");
  }
  if (auto margin_left = page_layout.margin.left) {
    result.append("margin-left:").append(margin_left->to_string()).append(";");
  }
  if (auto margin_bottom = page_layout.margin.bottom) {
    result.append("margin-bottom:")
        .append(margin_bottom->to_string())
        .append(";");
  }
  return result;
}

std::string html::translate_text_style(const TextStyle &text_style) {
  std::string result;
  if (auto font_name = text_style.font_name) {
    result.append("font-family:").append(font_name).append(";");
  }
  if (auto font_size = text_style.font_size) {
    result.append("font-size:").append(font_size->to_string()).append(";");
  }
  if (auto font_weight = text_style.font_weight) {
    result.append("font-weight:")
        .append(translate_font_weight(*font_weight))
        .append(";");
  }
  if (auto font_style = text_style.font_style) {
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
  if (auto font_shadow = text_style.font_shadow) {
    result.append("text-shadow:").append(*font_shadow).append(";");
  }
  if (auto font_color = text_style.font_color) {
    result.append("color:")
        .append(internal::html::color(*font_color))
        .append(";");
  }
  if (auto background_color = text_style.background_color) {
    result.append("background-color:")
        .append(internal::html::color(*background_color))
        .append(";");
  }
  return result;
}

std::string
html::translate_paragraph_style(const ParagraphStyle &paragraph_style) {
  std::string result;
  if (auto text_align = paragraph_style.text_align) {
    result.append("text-align:")
        .append(translate_text_align(*text_align))
        .append(";");
  }
  if (auto margin_right = paragraph_style.margin.right) {
    result.append("margin-right:")
        .append(margin_right->to_string())
        .append(";");
  }
  if (auto margin_top = paragraph_style.margin.top) {
    result.append("margin-top:").append(margin_top->to_string()).append(";");
  }
  if (auto margin_left = paragraph_style.margin.left) {
    result.append("margin-left:").append(margin_left->to_string()).append(";");
  }
  if (auto margin_bottom = paragraph_style.margin.bottom) {
    result.append("margin-bottom:")
        .append(margin_bottom->to_string())
        .append(";");
  }
  if (auto line_height = paragraph_style.line_height) {
    result.append("line-height:").append(line_height->to_string()).append(";");
  }
  return result;
}

std::string html::translate_table_style(const TableStyle &table_style) {
  std::string result;
  if (auto width = table_style.width) {
    result.append("width:").append(width->to_string()).append(";");
  }
  return result;
}

std::string
html::translate_table_column_style(const TableColumnStyle &table_column_style) {
  std::string result;
  if (auto width = table_column_style.width) {
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
  if (auto height = table_row_style.height) {
    result.append("height:").append(height->to_string()).append(";");
  }
  return result;
}

std::string
html::translate_table_cell_style(const TableCellStyle &table_cell_style) {
  std::string result;
  if (auto horizontal_align = table_cell_style.horizontal_align) {
    result.append("text-align:")
        .append(translate_horizontal_align(*horizontal_align))
        .append(";");
  }
  if (auto vertical_align = table_cell_style.vertical_align) {
    result.append("vertical-align:")
        .append(translate_vertical_align(*vertical_align))
        .append(";");
  }
  if (auto background_color = table_cell_style.background_color) {
    result.append("background-color:")
        .append(internal::html::color(*background_color))
        .append(";");
  }
  if (auto padding_right = table_cell_style.padding.right) {
    result.append("padding-right:")
        .append(padding_right->to_string())
        .append(";");
  }
  if (auto padding_top = table_cell_style.padding.top) {
    result.append("padding-top:").append(padding_top->to_string()).append(";");
  }
  if (auto padding_left = table_cell_style.padding.left) {
    result.append("padding-left:")
        .append(padding_left->to_string())
        .append(";");
  }
  if (auto padding_bottom = table_cell_style.padding.bottom) {
    result.append("padding-bottom:")
        .append(padding_bottom->to_string())
        .append(";");
  }
  if (auto border_right = table_cell_style.border.right) {
    result.append("border-right:").append(*border_right).append(";");
  }
  if (auto border_top = table_cell_style.border.top) {
    result.append("border-top:").append(*border_top).append(";");
  }
  if (auto border_left = table_cell_style.border.left) {
    result.append("border-left:").append(*border_left).append(";");
  }
  if (auto border_bottom = table_cell_style.border.bottom) {
    result.append("border-bottom:").append(*border_bottom).append(";");
  }
  if (auto text_rotation = table_cell_style.text_rotation;
      text_rotation && (*text_rotation != 0)) {
    // TODO covers only the -90° case
    result.append("writing-mode:vertical-lr;");
  }
  return result;
}

std::string html::translate_drawing_style(const GraphicStyle &graphic_style) {
  std::string result;
  if (auto stroke_width = graphic_style.stroke_width) {
    result.append("stroke-width:")
        .append(stroke_width->to_string())
        .append(";");
  }
  if (auto stroke_color = graphic_style.stroke_color) {
    result.append("stroke:")
        .append(internal::html::color(*stroke_color))
        .append(";");
  }
  if (auto fill_color = graphic_style.fill_color) {
    result.append("fill:")
        .append(internal::html::color(*fill_color))
        .append(";");
  }
  if (auto vertical_align = graphic_style.vertical_align) {
    if (vertical_align == VerticalAlign::middle) {
      result += "display:flex;justify-content:center;flex-direction:column;";
    }
    // TODO else log
  }
  return result;
}

std::string html::translate_frame_properties(const Frame &frame) {
  auto text_wrap = TextWrap::run_through;
  if (auto style = frame.style(); style.text_wrap) {
    text_wrap = *style.text_wrap;
  }

  std::string result;
  if (auto anchor_type = frame.anchor_type();
      anchor_type == AnchorType::as_char) {
    result += "display:inline-block;";
  } else if (text_wrap == TextWrap::before) {
    result += "display:block;";
    result += "float:right;clear:both;";
    result += "shape-outside:content-box;";
    if (auto x = frame.x()) {
      result += "margin-left:" + *x + ";";
    }
    if (auto y = frame.y()) {
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
    if (auto x = frame.x()) {
      result += "margin-left:" + *x + ";";
    }
    if (auto y = frame.y()) {
      result += "margin-top:" + *y + ";";
    }
  } else if (text_wrap == TextWrap::none) {
    result += "display:block;";
    if (auto x = frame.x()) {
      result += "margin-left:" + *x + ";";
    }
    if (auto y = frame.y()) {
      result += "margin-top:" + *y + ";";
    }
  } else {
    result += "display:block;";
    result += "position:absolute;";
    if (auto x = frame.x()) {
      result += "left:" + *x + ";";
    }
    if (auto y = frame.y()) {
      result += "top:" + *y + ";";
    }
  }
  if (auto width = frame.width()) {
    result += "width:" + *width + ";";
  }
  if (auto height = frame.height()) {
    result += "height:" + *height + ";";
  }
  if (auto z_index = frame.z_index()) {
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
  if (auto x = custom_shape.x()) {
    result += "left:" + *x + ";";
  } else {
    result += "left:0;";
  }
  if (auto y = custom_shape.y()) {
    result += "top:" + *y + ";";
  } else {
    result += "top:0;";
  }
  result += "width:" + custom_shape.width() + ";";
  result += "height:" + custom_shape.height() + ";";
  return result;
}

std::string html::optional_style_attribute(const std::string &style) {
  if (style.empty()) {
    return "";
  }
  return " style=\"" + style + "\"";
}

} // namespace odr::internal
