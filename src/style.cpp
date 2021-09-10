#include <odr/style.h>

namespace odr {

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
}

} // namespace odr
