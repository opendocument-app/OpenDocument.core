#include <fstream>
#include <internal/abstract/document.h>
#include <internal/common/file.h>
#include <internal/common/html.h>
#include <internal/common/table_cursor.h>
#include <internal/crypto/crypto_util.h>
#include <internal/svm/svm_file.h>
#include <internal/svm/svm_to_svg.h>
#include <internal/util/stream_util.h>
#include <nlohmann/json.hpp>
#include <odr/document.h>
#include <odr/exceptions.h>
#include <odr/file.h>
#include <odr/html.h>
#include <odr/style.h>
#include <sstream>

using namespace odr::internal;

namespace odr {

namespace {

const char *translate_text_align(const TextAlign text_align) {
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

const char *translate_vertical_align(const VerticalAlign vertical_align) {
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

const char *translate_font_weight(const FontWeight font_weight) {
  switch (font_weight) {
  case FontWeight::normal:
    return "normal";
  case FontWeight::bold:
    return "bold";
  default:
    return ""; // TODO log
  }
}

const char *translate_font_style(const FontStyle font_style) {
  switch (font_style) {
  case FontStyle::normal:
    return "normal";
  case FontStyle::italic:
    return "italic";
  default:
    return ""; // TODO log
  }
}

std::string translate_color(const Color &color) {
  // TODO alpha
  std::stringstream ss;
  ss << "#";
  ss << std::setw(6) << std::setfill('0') << std::hex << color.rgb();
  return ss.str();
}

std::string translate_outer_page_style(const PageLayout &page_layout) {
  std::string result;
  if (auto width = page_layout.width) {
    result.append("width:").append(width->to_string()).append(";");
  }
  if (auto height = page_layout.height) {
    result.append("height:").append(height->to_string()).append(";");
  }
  return result;
}

std::string translate_inner_page_style(const PageLayout &page_layout) {
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

std::string translate_text_style(const TextStyle &text_style) {
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
  if (text_style.font_underline) {
    result += "text-decoration:underline;";
  }
  if (text_style.font_line_through) {
    result += "text-decoration:line-through;";
  }
  if (auto font_shadow = text_style.font_shadow) {
    result.append("text-shadow:").append(*font_shadow).append(";");
  }
  if (auto font_color = text_style.font_color) {
    result.append("color:").append(translate_color(*font_color)).append(";");
  }
  if (auto background_color = text_style.background_color) {
    result.append("background-color:")
        .append(translate_color(*background_color))
        .append(";");
  }
  return result;
}

std::string translate_paragraph_style(const ParagraphStyle &paragraph_style) {
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
  return result;
}

std::string translate_table_style(const TableStyle &table_style) {
  std::string result;
  if (auto width = table_style.width) {
    result.append("width:").append(width->to_string()).append(";");
  }
  return result;
}

std::string translate_table_row_style(const TableRowStyle &table_row_style) {
  std::string result;
  // TODO that does not work with HTML; height would need to be applied to the
  // cells
  if (auto height = table_row_style.height) {
    result.append("height:").append(height->to_string()).append(";");
  }
  return result;
}

std::string translate_table_cell_style(const TableCellStyle &table_cell_style) {
  std::string result;
  if (auto vertical_align = table_cell_style.vertical_align) {
    result.append("vertical-align:")
        .append(translate_vertical_align(*vertical_align))
        .append(";");
  }
  if (auto background_color = table_cell_style.background_color) {
    result.append("background-color:")
        .append(translate_color(*background_color))
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
  return result;
}

std::string translate_drawing_style(const GraphicStyle &graphic_style) {
  std::string result;
  if (auto stroke_width = graphic_style.stroke_width) {
    result.append("stroke-width:")
        .append(stroke_width->to_string())
        .append(";");
  }
  if (auto stroke_color = graphic_style.stroke_color) {
    result.append("stroke:").append(translate_color(*stroke_color)).append(";");
  }
  if (auto fill_color = graphic_style.fill_color) {
    result.append("fill:").append(translate_color(*fill_color)).append(";");
  }
  if (auto vertical_align = graphic_style.vertical_align) {
    if (vertical_align == VerticalAlign::middle) {
      result += "display:flex;justify-content:center;flex-direction:column;";
    }
    // TODO else log
  }
  return result;
}

std::string translate_frame_properties(const Element::Frame &frame) {
  std::string result;
  if (auto anchor_type = frame.anchor_type();
      anchor_type && (*anchor_type == "as-char")) {
    result += "display:inline-block;";
  } else if (anchor_type && (*anchor_type == "char")) {
    result += "display:block;";
    result += "float:left;clear:both;";
    result += "shape-outside:content-box;";
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

std::string translate_rect_properties(const Element::Rect &rect) {
  std::string result;
  result += "position:absolute;";
  result += "left:" + rect.x() + ";";
  result += "top:" + rect.y() + ";";
  result += "width:" + rect.width() + ";";
  result += "height:" + rect.height() + ";";
  return result;
}

std::string translate_circle_properties(const Element::Circle &circle) {
  std::string result;
  result += "position:absolute;";
  result += "left:" + circle.x() + ";";
  result += "top:" + circle.y() + ";";
  result += "width:" + circle.width() + ";";
  result += "height:" + circle.height() + ";";
  return result;
}

std::string
translate_custom_shape_properties(const Element::CustomShape &custom_shape) {
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

std::string optional_style_attribute(const std::string &style) {
  if (style.empty()) {
    return "";
  }
  return " style=\"" + style + "\"";
}

void translate_children(DocumentCursor &cursor, std::ostream &out,
                        const HtmlConfig &config);
void translate_element(DocumentCursor &cursor, std::ostream &out,
                       const HtmlConfig &config);

void translate_text(const DocumentCursor &cursor, std::ostream &out) {
  auto text = cursor.element().text();

  out << "<span";
  if (auto style = text.style()) {
    out << optional_style_attribute(translate_text_style(*style));
  }
  out << ">";
  out << common::html::escape_text(cursor.element().text().value());
  out << "</span>";
}

void translate_paragraph(DocumentCursor &cursor, std::ostream &out,
                         const HtmlConfig &config) {
  auto paragraph = cursor.element().paragraph();

  out << "<p";
  if (auto style = paragraph.style()) {
    out << optional_style_attribute(translate_paragraph_style(*style));
  }
  out << ">";
  translate_children(cursor, out, config);
  out << "</p>";
}

void translate_span(DocumentCursor &cursor, std::ostream &out,
                    const HtmlConfig &config) {
  out << "<span>";
  translate_children(cursor, out, config);
  out << "</span>";
}

void translate_link(DocumentCursor &cursor, std::ostream &out,
                    const HtmlConfig &config) {
  auto link = cursor.element().link();

  out << "<a href=\"";
  out << link.href();
  out << "\">";
  translate_children(cursor, out, config);
  out << "</a>";
}

void translate_bookmark(DocumentCursor &cursor, std::ostream &out,
                        const HtmlConfig & /*config*/) {
  out << "<a id=\"";
  out << cursor.element().bookmark().name();
  out << "\"></a>";
}

void translate_list(DocumentCursor &cursor, std::ostream &out,
                    const HtmlConfig &config) {
  out << "<ul>";
  cursor.for_each_child([&](DocumentCursor &cursor, const std::uint32_t) {
    auto list_item = cursor.element().list_item();

    out << "<li";
    if (auto style = list_item.style()) {
      out << optional_style_attribute(translate_text_style(*style));
    }
    out << ">";
    translate_children(cursor, out, config);
    out << "</li>";
  });
  out << "</ul>";
}

void translate_table(DocumentCursor &cursor, std::ostream &out,
                     const HtmlConfig &config) {
  // TODO table column width does not work
  // TODO table row height does not work
  auto table = cursor.element().table();

  out << "<table";
  if (auto style = table.style()) {
    out << optional_style_attribute(translate_table_style(*style));
  }
  out << R"( cellpadding="0" border="0" cellspacing="0")";
  out << ">";

  cursor.for_each_column([&](DocumentCursor &, const std::uint32_t) {
    out << "<col>";
    return true;
  });

  cursor.for_each_row([&](DocumentCursor &cursor, const std::uint32_t) {
    auto table_row = cursor.element().table_row();

    out << "<tr";
    if (auto style = table_row.style()) {
      out << optional_style_attribute(translate_table_row_style(*style));
    }
    out << ">";

    cursor.for_each_cell([&](DocumentCursor &cursor, const std::uint32_t) {
      auto table_cell = cursor.element().table_cell();

      if (!table_cell.covered()) {
        auto cell_span = table_cell.span();

        out << "<td";
        if (cell_span.rows > 1) {
          out << " rowspan=\"" << cell_span.rows << "\"";
        }
        if (cell_span.columns > 1) {
          out << " colspan=\"" << cell_span.columns << "\"";
        }
        if (auto style = table_cell.style()) {
          out << optional_style_attribute(translate_table_cell_style(*style));
        }
        out << ">";
        translate_children(cursor, out, config);
        out << "</td>";
      }

      return true;
    });

    out << "</tr>";

    return true;
  });

  out << "</table>";
}

void translate_image(DocumentCursor &cursor, std::ostream &out,
                     const HtmlConfig & /*config*/) {
  out << "<img style=\"position:absolute;left:0;top:0;width:100%;height:100%\"";
  out << " alt=\"Error: image not found or unsupported\"";
  out << " src=\"";

  if (cursor.element().image().internal()) {
    auto image_file = cursor.element().image().file().value();

    // TODO use stream
    std::string image_data;

    try {
      // try svm
      // TODO `impl()` might be a bit dirty
      auto image_file_impl = image_file.impl();
      // TODO memory file might not be necessary; other istreams didn't support
      // `tellg`
      svm::SvmFile svm_file(
          std::make_shared<common::MemoryFile>(*image_file_impl));
      std::ostringstream svg_out;
      svm::Translator::svg(svm_file, svg_out);
      image_data = svg_out.str();
      out << "data:image/svg+xml;base64, ";
    } catch (...) {
      // else we guess that it is a usual image
      image_data = util::stream::read(*image_file.read());
      // TODO hacky - `image/jpg` works for all common image types in chrome
      out << "data:image/jpg;base64, ";
    }

    // TODO stream
    out << crypto::util::base64_encode(image_data);
  } else {
    out << cursor.element().image().href();
  }

  out << "\">";
}

void translate_frame(DocumentCursor &cursor, std::ostream &out,
                     const HtmlConfig &config) {
  auto frame = cursor.element().frame();

  // TODO choosing <span> by default because it is valid inside <p>;
  // alternatives?
  const bool span = frame.anchor_type().has_value();
  if (span) {
    out << "<span";
  } else {
    out << "<div";
  }

  out << optional_style_attribute(
      translate_frame_properties(frame) +
      (frame.style() ? translate_drawing_style(*frame.style()) : ""));
  out << ">";
  translate_children(cursor, out, config);

  if (span) {
    out << "</span>";
  } else {
    out << "</div>";
  }
}

void translate_rect(DocumentCursor &cursor, std::ostream &out,
                    const HtmlConfig &config) {
  auto rect = cursor.element().rect();

  out << "<div";
  out << optional_style_attribute(
      translate_rect_properties(cursor.element().rect()) +
      (rect.style() ? translate_drawing_style(*rect.style()) : ""));
  out << ">";
  translate_children(cursor, out, config);
  out << R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" overflow="visible" preserveAspectRatio="none" style="z-index:-1;width:inherit;height:inherit;position:absolute;top:0;left:0;padding:inherit;"><rect x="0" y="0" width="100%" height="100%" /></svg>)";
  out << "</div>";
}

void translate_line(DocumentCursor &cursor, std::ostream &out,
                    const HtmlConfig & /*config*/) {
  auto line = cursor.element().line();

  out << R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" overflow="visible")";
  out << optional_style_attribute(
      "z-index:-1;position:absolute;top:0;left:0;" +
      (line.style() ? translate_drawing_style(*line.style()) : ""));
  out << ">";

  out << "<line";
  out << " x1=\"" << cursor.element().line().x1() << "\" y1=\""
      << cursor.element().line().y1() << "\"";
  out << " x2=\"" << cursor.element().line().x2() << "\" y2=\""
      << cursor.element().line().y2() << "\"";
  out << " />";

  out << "</svg>";
}

void translate_circle(DocumentCursor &cursor, std::ostream &out,
                      const HtmlConfig &config) {
  auto circle = cursor.element().circle();

  out << "<div";
  out << optional_style_attribute(
      translate_circle_properties(cursor.element().circle()) +
      (circle.style() ? translate_drawing_style(*circle.style()) : ""));
  out << ">";
  translate_children(cursor, out, config);
  out << R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" overflow="visible" preserveAspectRatio="none" style="z-index:-1;width:inherit;height:inherit;position:absolute;top:0;left:0;padding:inherit;"><circle cx="50%" cy="50%" r="50%" /></svg>)";
  out << "</div>";
}

void translate_custom_shape(DocumentCursor &cursor, std::ostream &out,
                            const HtmlConfig &config) {
  auto custom_shape = cursor.element().custom_shape();

  out << "<div";
  out << optional_style_attribute(
      translate_custom_shape_properties(cursor.element().custom_shape()) +
      (custom_shape.style() ? translate_drawing_style(*custom_shape.style())
                            : ""));
  out << ">";
  translate_children(cursor, out, config);
  // TODO draw shape in svg
  out << "</div>";
}

void translate_children(DocumentCursor &cursor, std::ostream &out,
                        const HtmlConfig &config) {
  cursor.for_each_child([&](DocumentCursor &cursor, const std::uint32_t) {
    translate_element(cursor, out, config);
  });
}

void translate_element(DocumentCursor &cursor, std::ostream &out,
                       const HtmlConfig &config) {
  if (cursor.element_type() == ElementType::text) {
    translate_text(cursor, out);
  } else if (cursor.element_type() == ElementType::line_break) {
    out << "<br>";
  } else if (cursor.element_type() == ElementType::paragraph) {
    translate_paragraph(cursor, out, config);
  } else if (cursor.element_type() == ElementType::span) {
    translate_span(cursor, out, config);
  } else if (cursor.element_type() == ElementType::link) {
    translate_link(cursor, out, config);
  } else if (cursor.element_type() == ElementType::bookmark) {
    translate_bookmark(cursor, out, config);
  } else if (cursor.element_type() == ElementType::list) {
    translate_list(cursor, out, config);
  } else if (cursor.element_type() == ElementType::table) {
    translate_table(cursor, out, config);
  } else if (cursor.element_type() == ElementType::frame) {
    translate_frame(cursor, out, config);
  } else if (cursor.element_type() == ElementType::image) {
    translate_image(cursor, out, config);
  } else if (cursor.element_type() == ElementType::rect) {
    translate_rect(cursor, out, config);
  } else if (cursor.element_type() == ElementType::line) {
    translate_line(cursor, out, config);
  } else if (cursor.element_type() == ElementType::circle) {
    translate_circle(cursor, out, config);
  } else if (cursor.element_type() == ElementType::custom_shape) {
    translate_custom_shape(cursor, out, config);
  } else if (cursor.element_type() == ElementType::group) {
    translate_children(cursor, out, config);
  } else {
    // TODO log
  }
}

void translate_master_page(DocumentCursor &cursor, std::ostream &out,
                           const HtmlConfig &config) {
  if (!cursor.move_to_master_page()) {
    return;
  }

  cursor.for_each_child([&](DocumentCursor &cursor, const std::uint32_t) {
    // TODO filter placeholders
    translate_element(cursor, out, config);
  });

  cursor.move_to_parent();
}

void translate_sheet(DocumentCursor &cursor, std::ostream &out,
                     const HtmlConfig &config) {
  // TODO table column width does not work
  // TODO table row height does not work

  out << "<table";
  out << R"( cellpadding="0" border="0" cellspacing="0")";
  out << ">";

  auto table = cursor.element().table();
  auto sheet = cursor.element().sheet();
  auto dimensions = table.dimensions();
  std::uint32_t end_row = dimensions.rows;
  std::uint32_t end_column = dimensions.columns;
  if (config.table_limit) {
    end_row = config.table_limit->rows;
    end_column = config.table_limit->columns;
  }
  if (config.table_limit_by_content) {
    const auto content = sheet.content(config.table_limit);
    end_row = content.rows;
    end_column = content.columns;
  }

  std::uint32_t column_index = 0;
  std::uint32_t row_index = 0;

  out << "<col>";

  column_index = 0;
  cursor.for_each_column([&](DocumentCursor &, const std::uint32_t) {
    out << "<col>";

    ++column_index;
    return column_index < end_column;
  });

  {
    out << "<tr>";
    out << "<td style=\"width:30px;height:20px;\"/>";

    column_index = 0;
    cursor.for_each_column([&](DocumentCursor &, const std::uint32_t) {
      auto table_column = cursor.element().table_column();
      auto table_column_style = table_column.style();

      out << "<td style=\"text-align:center;vertical-align:middle;";
      if (table_column_style) {
        if (auto width = table_column_style->width) {
          out << "width:" << width->to_string() << ";";
          out << "max-width:" << width->to_string() << ";";
        }
      }
      out << "\">";
      out << common::TablePosition::to_column_string(column_index);
      out << "</td>";

      ++column_index;
      return column_index < end_column;
    });

    out << "</tr>";
  }

  cursor.for_each_row([&](DocumentCursor &cursor, const std::uint32_t) {
    auto table_row = cursor.element().table_row();
    auto table_row_style = table_row.style();

    out << "<tr";
    if (table_row_style) {
      out << optional_style_attribute(
          translate_table_row_style(*table_row_style));
    }
    out << ">";

    out << "<td style=\"text-align:center;vertical-align:middle;";
    if (table_row_style) {
      if (auto height = table_row_style->height) {
        out << "height:" << height->to_string() << ";";
        out << "max-height:" << height->to_string() << ";";
      }
    }
    out << "\">";
    out << common::TablePosition::to_row_string(row_index);
    out << "</td>";

    column_index = 0;
    cursor.for_each_cell([&](DocumentCursor &cursor, const std::uint32_t) {
      auto table_cell = cursor.element().table_cell();

      if (!table_cell.covered()) {
        auto cell_span = table_cell.span();

        out << "<td";
        if (cell_span.rows > 1) {
          out << " rowspan=\"" << cell_span.rows << "\"";
        }
        if (cell_span.columns > 1) {
          out << " colspan=\"" << cell_span.columns << "\"";
        }
        if (auto style = table_cell.style()) {
          out << optional_style_attribute(translate_table_cell_style(*style));
        }
        out << ">";
        translate_children(cursor, out, config);
        out << "</td>";
      }

      ++column_index;
      return column_index < end_column;
    });

    out << "</tr>";

    ++row_index;
    return row_index < end_row;
  });

  out << "</table>";
}

void translate_text_document(DocumentCursor &cursor, std::ostream &out,
                             const HtmlConfig &config) {
  auto element = cursor.element().text_root();

  if (config.text_document_margin) {
    out << "<div";
    out << optional_style_attribute(
        translate_outer_page_style(element.page_layout()));
    out << ">";
    out << "<div";
    out << optional_style_attribute(
        translate_inner_page_style(element.page_layout()));
    out << ">";
    translate_children(cursor, out, config);
    out << "</div>";
    out << "</div>";
  } else {
    translate_children(cursor, out, config);
  }
}

void translate_presentation(DocumentCursor &cursor, std::ostream &out,
                            const HtmlConfig &config) {
  // TODO indexing is kind of ugly here and duplicated
  std::uint32_t begin_entry = config.entry_offset;
  std::optional<std::uint32_t> end_entry;

  if (config.entry_count > 0) {
    end_entry = begin_entry + config.entry_count;
  }

  cursor.for_each_child([&](DocumentCursor &cursor, const std::uint32_t i) {
    if ((i < begin_entry) || (end_entry && (i >= end_entry))) {
      return;
    }
    auto slide = cursor.element().slide();

    out << "<div";
    out << optional_style_attribute(
        translate_outer_page_style(slide.page_layout()));
    out << ">";
    out << "<div";
    out << optional_style_attribute(
        translate_inner_page_style(slide.page_layout()));
    out << ">";
    translate_master_page(cursor, out, config);
    translate_children(cursor, out, config);
    out << "</div>";
    out << "</div>";
  });
}

void translate_spreadsheet(DocumentCursor &cursor, std::ostream &out,
                           const HtmlConfig &config) {
  std::uint32_t begin_entry = config.entry_offset;
  std::optional<std::uint32_t> end_entry;

  if (config.entry_count > 0) {
    end_entry = begin_entry + config.entry_count;
  }

  cursor.for_each_child([&](DocumentCursor &cursor, const std::uint32_t i) {
    if ((i < begin_entry) || (end_entry && (i >= end_entry))) {
      return;
    }

    translate_sheet(cursor, out, config);
  });
}

void translate_drawing(DocumentCursor &cursor, std::ostream &out,
                       const HtmlConfig &config) {
  std::uint32_t begin_entry = config.entry_offset;
  std::optional<std::uint32_t> end_entry;

  if (config.entry_count > 0) {
    end_entry = begin_entry + config.entry_count;
  }

  cursor.for_each_child([&](DocumentCursor &cursor, const std::uint32_t i) {
    if ((i < begin_entry) || (end_entry && (i >= end_entry))) {
      return;
    }
    auto page = cursor.element().page();

    out << "<div";
    out << optional_style_attribute(
        translate_outer_page_style(page.page_layout()));
    out << ">";
    out << "<div";
    out << optional_style_attribute(
        translate_inner_page_style(page.page_layout()));
    out << ">";
    translate_master_page(cursor, out, config);
    translate_children(cursor, out, config);
    out << "</div>";
    out << "</div>";
  });
}
} // namespace

HtmlConfig html::parse_config(const std::string &path) {
  HtmlConfig result;

  auto json = nlohmann::json::parse(std::ifstream(path));
  // TODO assign config values

  return result;
}

void html::translate(const Document &document,
                     const std::string & /*document_identifier*/,
                     const std::string &path, const HtmlConfig &config) {
  std::ofstream out(path);
  if (!out.is_open()) {
    return; // TODO throw
  }

  out << common::html::doctype();
  out << "<html><head>";
  out << common::html::default_headers();
  out << "<style>";
  out << common::html::default_style();
  if (document.document_type() == DocumentType::SPREADSHEET) {
    out << common::html::default_spreadsheet_style();
  }
  out << "</style>";
  out << "</head>";

  out << "<body " << common::html::body_attributes(config) << ">";

  auto cursor = document.root_element();

  if (document.document_type() == DocumentType::TEXT) {
    translate_text_document(cursor, out, config);
  } else if (document.document_type() == DocumentType::PRESENTATION) {
    translate_presentation(cursor, out, config);
  } else if (document.document_type() == DocumentType::SPREADSHEET) {
    translate_spreadsheet(cursor, out, config);
  } else if (document.document_type() == DocumentType::DRAWING) {
    translate_drawing(cursor, out, config);
  } else {
    // TODO throw?
  }

  out << "</body>";

  out << "<script>";
  out << common::html::default_script();
  out << "</script>";
  out << "</html>";
}

void html::edit(const Document & /*document*/,
                const std::string & /*document_identifier*/,
                const std::string & /*diff*/) {
  throw UnsupportedOperation();
}

} // namespace odr
