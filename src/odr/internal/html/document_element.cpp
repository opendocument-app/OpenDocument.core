#include <odr/document_element.hpp>

#include <odr/document_path.hpp>
#include <odr/html.hpp>

#include <odr/internal/common/table_range.hpp>
#include <odr/internal/html/common.hpp>
#include <odr/internal/html/document_element.hpp>
#include <odr/internal/html/document_style.hpp>
#include <odr/internal/html/image_file.hpp>

#include "odr/internal/common/table_cursor.hpp"
#include <iostream>

namespace odr::internal {

void html::translate_children(ElementRange range, std::ostream &out,
                              const HtmlConfig &config) {
  for (auto child : range) {
    translate_element(child, out, config);
  }
}

void html::translate_element(Element element, std::ostream &out,
                             const HtmlConfig &config) {
  if (element.type() == ElementType::text) {
    translate_text(element, out, config);
  } else if (element.type() == ElementType::line_break) {
    translate_line_break(element, out, config);
  } else if (element.type() == ElementType::paragraph) {
    translate_paragraph(element, out, config);
  } else if (element.type() == ElementType::span) {
    translate_span(element, out, config);
  } else if (element.type() == ElementType::link) {
    translate_link(element, out, config);
  } else if (element.type() == ElementType::bookmark) {
    translate_bookmark(element, out, config);
  } else if (element.type() == ElementType::list) {
    translate_list(element, out, config);
  } else if (element.type() == ElementType::list_item) {
    translate_list_item(element, out, config);
  } else if (element.type() == ElementType::table) {
    translate_table(element, out, config);
  } else if (element.type() == ElementType::frame) {
    translate_frame(element, out, config);
  } else if (element.type() == ElementType::image) {
    translate_image(element, out, config);
  } else if (element.type() == ElementType::rect) {
    translate_rect(element, out, config);
  } else if (element.type() == ElementType::line) {
    translate_line(element, out, config);
  } else if (element.type() == ElementType::circle) {
    translate_circle(element, out, config);
  } else if (element.type() == ElementType::custom_shape) {
    translate_custom_shape(element, out, config);
  } else if (element.type() == ElementType::group) {
    translate_children(element.children(), out, config);
  } else {
    // TODO log
  }
}

void html::translate_slide(Element element, std::ostream &out,
                           const HtmlConfig &config) {
  auto slide = element.slide();

  out << "<div";
  out << optional_style_attribute(
      translate_outer_page_style(slide.page_layout()));
  out << ">";
  out << "<div";
  out << optional_style_attribute(
      translate_inner_page_style(slide.page_layout()));
  out << ">";
  translate_master_page(slide.master_page(), out, config);
  translate_children(slide.children(), out, config);
  out << "</div>";
  out << "</div>";
}

void html::translate_sheet(Element element, std::ostream &out,
                           const HtmlConfig &config) {
  // TODO table column width does not work
  // TODO table row height does not work

  out << "<table";
  out << R"( cellpadding="0" border="0" cellspacing="0")";
  out << ">";

  auto sheet = element.sheet();
  auto dimensions = sheet.dimensions();
  std::uint32_t end_column = dimensions.columns;
  std::uint32_t end_row = dimensions.rows;
  if (config.spreadsheet_limit_by_content) {
    const auto content = sheet.content(config.spreadsheet_limit);
    end_column = content.columns;
    end_row = content.rows;
  }
  if (config.spreadsheet_limit) {
    end_column = std::min(end_column, config.spreadsheet_limit->columns);
    end_row = std::min(end_row, config.spreadsheet_limit->rows);
  }
  end_column = std::max(1u, end_column);
  end_row = std::max(1u, end_row);

  out << "<col>";

  for (std::uint32_t column_index = 0; column_index < end_column;
       ++column_index) {
    auto table_column = sheet.column(column_index);
    auto table_column_style = table_column.style();

    out << "<col";
    out << optional_style_attribute(
        translate_table_column_style(table_column_style));
    out << ">";
  }

  {
    out << "<tr>";
    out << "<td style=\"width:30px;height:20px;\"/>";

    for (std::uint32_t column_index = 0; column_index < end_column;
         ++column_index) {
      out << "<td style=\"text-align:center;vertical-align:middle;\">";
      out << common::TablePosition::to_column_string(column_index);
      out << "</td>";
    }

    out << "</tr>";
  }

  common::TableCursor cursor;
  for (std::uint32_t row_index = cursor.row(); row_index < end_row;
       row_index = cursor.row()) {
    auto table_row = sheet.row(row_index);
    auto table_row_style = table_row.style();

    out << "<tr";
    out << optional_style_attribute(translate_table_row_style(table_row_style));
    out << ">";

    out << "<td style=\"text-align:center;vertical-align:middle;";
    if (auto height = table_row_style.height) {
      out << "height:" << height->to_string() << ";";
      out << "max-height:" << height->to_string() << ";";
    }
    out << "\">";
    out << common::TablePosition::to_row_string(row_index);
    out << "</td>";

    for (std::uint32_t column_index = cursor.column();
         column_index < end_column; column_index = cursor.column()) {
      auto cell = sheet.cell(column_index, row_index);

      if (cell.is_covered()) {
        continue;
      }

      // TODO looks a bit odd to query the same (col, row) all the time. maybe
      // there could be a struct to get all the info?
      auto cell_style = cell.style();
      auto cell_span = cell.span();
      auto cell_value_type = cell.value_type();
      auto cell_elements = cell.children();

      out << "<td";
      if (cell_span.columns > 1) {
        out << " colspan=\"" << cell_span.columns << "\"";
      }
      if (cell_span.rows > 1) {
        out << " rowspan=\"" << cell_span.rows << "\"";
      }
      out << optional_style_attribute(translate_table_cell_style(cell_style));
      if (cell_value_type == ValueType::float_number) {
        out << " class=\"odr-value-type-float\"";
      }
      out << ">";
      if ((column_index == 0) && (row_index == 0)) {
        for (auto shape : sheet.shapes()) {
          translate_element(shape, out, config);
        }
      }
      translate_children(cell_elements, out, config);
      out << "</td>";

      cursor.add_cell(cell_span.columns, cell_span.rows);
    }

    out << "</tr>";

    cursor.add_row();
  }

  out << "</table>";
}

void html::translate_page(Element element, std::ostream &out,
                          const HtmlConfig &config) {
  auto page = element.page();

  out << "<div";
  out << optional_style_attribute(
      translate_outer_page_style(page.page_layout()));
  out << ">";
  out << "<div";
  out << optional_style_attribute(
      translate_inner_page_style(page.page_layout()));
  out << ">";
  translate_master_page(page.master_page(), out, config);
  translate_children(page.children(), out, config);
  out << "</div>";
  out << "</div>";
}

void html::translate_master_page(MasterPage element, std::ostream &out,
                                 const HtmlConfig &config) {
  for (auto child : element.children()) {
    // TODO filter placeholders
    translate_element(child, out, config);
  }
}

void html::translate_text(const Element element, std::ostream &out,
                          const HtmlConfig &config) {
  auto text = element.text();

  out << "<x-s";
  out << optional_style_attribute(translate_text_style(text.style()));
  if (config.editable && element.is_editable()) {
    out << " contenteditable=\"true\"";
    out << " data-odr-path=\"" << DocumentPath::extract(element).to_string()
        << "\"";
  }
  out << ">";
  out << internal::html::escape_text(text.content());
  out << "</x-s>";
}

void html::translate_line_break(Element element, std::ostream &out,
                                const HtmlConfig &) {
  auto line_break = element.line_break();

  out << "<br>";
  out << "<x-s";
  out << optional_style_attribute(translate_text_style(line_break.style()));
  out << "></x-s>";
}

void html::translate_paragraph(Element element, std::ostream &out,
                               const HtmlConfig &config) {
  auto paragraph = element.paragraph();
  auto text_style_attribute =
      optional_style_attribute(translate_text_style(paragraph.text_style()));

  out << "<x-p";
  out << optional_style_attribute(translate_paragraph_style(paragraph.style()));
  out << ">";
  translate_children(paragraph.children(), out, config);
  if (paragraph.first_child()) {
    // TODO if element is content (e.g. bookmark does not count)

    // TODO example `encrypted-exception-3$aabbcc$.odt` at the very bottom
    // TODO has a missing line break after "As the result of the project we ..."

    // TODO example `style-missing+image-1.odt` first paragraph has no height
  } else {
    out << "<x-s" << text_style_attribute << "></x-s>";
  }
  out << "<wbr>";
  out << "</x-p>";
}

void html::translate_span(Element element, std::ostream &out,
                          const HtmlConfig &config) {
  auto span = element.span();

  out << "<x-s";
  out << optional_style_attribute(translate_text_style(span.style()));
  out << ">";
  translate_children(span.children(), out, config);
  out << "</x-s>";
}

void html::translate_link(Element element, std::ostream &out,
                          const HtmlConfig &config) {
  auto link = element.link();

  out << "<a href=\"";
  out << link.href();
  out << "\">";
  translate_children(link.children(), out, config);
  out << "</a>";
}

void html::translate_bookmark(Element element, std::ostream &out,
                              const HtmlConfig & /*config*/) {
  auto bookmark = element.bookmark();

  out << "<a id=\"";
  out << bookmark.name();
  out << "\"></a>";
}

void html::translate_list(Element element, std::ostream &out,
                          const HtmlConfig &config) {
  out << "<ul>";
  translate_children(element.children(), out, config);
  out << "</ul>";
}

void html::translate_list_item(Element element, std::ostream &out,
                               const HtmlConfig &config) {
  auto list_item = element.list_item();

  out << "<li";
  out << optional_style_attribute(translate_text_style(list_item.style()));
  out << ">";
  translate_children(list_item.children(), out, config);
  out << "</li>";
}

void html::translate_table(Element element, std::ostream &out,
                           const HtmlConfig &config) {
  // TODO table column width does not work
  // TODO table row height does not work
  auto table = element.table();

  out << "<table";
  out << optional_style_attribute(translate_table_style(table.style()));
  out << R"( cellpadding="0" border="0" cellspacing="0")";
  out << ">";

  for (auto column : table.columns()) {
    auto table_column = column.table_column();

    out << "<col";
    out << optional_style_attribute(
        translate_table_column_style(table_column.style()));
    out << ">";
  }

  for (auto row : table.rows()) {
    auto table_row = row.table_row();

    out << "<tr";
    out << optional_style_attribute(
        translate_table_row_style(table_row.style()));
    out << ">";

    for (auto cell : table_row.children()) {
      auto table_cell = cell.table_cell();

      if (!table_cell.is_covered()) {
        auto cell_span = table_cell.span();

        out << "<td";
        if (cell_span.rows > 1) {
          out << " rowspan=\"" << cell_span.rows << "\"";
        }
        if (cell_span.columns > 1) {
          out << " colspan=\"" << cell_span.columns << "\"";
        }
        out << optional_style_attribute(
            translate_table_cell_style(table_cell.style()));
        out << ">";
        translate_children(cell.children(), out, config);
        out << "</td>";
      }
    }

    out << "</tr>";
  }

  out << "</table>";
}

void html::translate_image(Element element, std::ostream &out,
                           const HtmlConfig &config) {
  auto image = element.image();

  out << "<img style=\"position:absolute;left:0;top:0;width:100%;height:100%\"";
  out << " alt=\"Error: image not found or unsupported\"";
  out << " src=\"";

  if (image.is_internal()) {
    translate_image_src(image.file().value(), out, config);
  } else {
    out << image.href();
  }

  out << "\">";
}

void html::translate_frame(Element element, std::ostream &out,
                           const HtmlConfig &config) {
  auto frame = element.frame();
  auto style = frame.style();

  out << "<div";

  out << optional_style_attribute(translate_frame_properties(frame) +
                                  translate_drawing_style(style));
  out << ">";
  translate_children(frame.children(), out, config);

  out << "</div>";
}

void html::translate_rect(Element element, std::ostream &out,
                          const HtmlConfig &config) {
  auto rect = element.rect();

  out << "<div";
  out << optional_style_attribute(translate_rect_properties(rect) +
                                  translate_drawing_style(rect.style()));
  out << ">";
  translate_children(rect.children(), out, config);
  out << R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" overflow="visible" preserveAspectRatio="none" style="z-index:-1;width:inherit;height:inherit;position:absolute;top:0;left:0;padding:inherit;"><rect x="0" y="0" width="100%" height="100%" /></svg>)";
  out << "</div>";
}

void html::translate_line(Element element, std::ostream &out,
                          const HtmlConfig & /*config*/) {
  auto line = element.line();

  out << R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" overflow="visible")";
  out << optional_style_attribute("z-index:-1;position:absolute;top:0;left:0;" +
                                  translate_drawing_style(line.style()));
  out << ">";

  out << "<line";
  out << " x1=\"" << line.x1() << "\" y1=\"" << line.y1() << "\"";
  out << " x2=\"" << line.x2() << "\" y2=\"" << line.y2() << "\"";
  out << " />";

  out << "</svg>";
}

void html::translate_circle(Element element, std::ostream &out,
                            const HtmlConfig &config) {
  auto circle = element.circle();

  out << "<div";
  out << optional_style_attribute(translate_circle_properties(circle) +
                                  translate_drawing_style(circle.style()));
  out << ">";
  translate_children(circle.children(), out, config);
  out << R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" overflow="visible" preserveAspectRatio="none" style="z-index:-1;width:inherit;height:inherit;position:absolute;top:0;left:0;padding:inherit;"><circle cx="50%" cy="50%" r="50%" /></svg>)";
  out << "</div>";
}

void html::translate_custom_shape(Element element, std::ostream &out,
                                  const HtmlConfig &config) {
  auto custom_shape = element.custom_shape();

  out << "<div";
  out << optional_style_attribute(
      translate_custom_shape_properties(custom_shape) +
      translate_drawing_style(custom_shape.style()));
  out << ">";
  translate_children(custom_shape.children(), out, config);
  // TODO draw shape in svg
  out << "</div>";
}

} // namespace odr::internal
