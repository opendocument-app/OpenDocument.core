#include <internal/common/file.h>
#include <internal/common/table_range.h>
#include <internal/crypto/crypto_util.h>
#include <internal/html/common.h>
#include <internal/html/document_element.h>
#include <internal/html/document_style.h>
#include <internal/svm/svm_file.h>
#include <internal/svm/svm_to_svg.h>
#include <internal/util/stream_util.h>
#include <iostream>
#include <odr/document_cursor.h>
#include <odr/document_element.h>
#include <odr/html.h>
#include <sstream>

namespace odr::internal {

void html::translate_children(DocumentCursor &cursor, std::ostream &out,
                              const HtmlConfig &config) {
  cursor.for_each_child([&](DocumentCursor &cursor, const std::uint32_t) {
    translate_element(cursor, out, config);
  });
}

void html::translate_element(DocumentCursor &cursor, std::ostream &out,
                             const HtmlConfig &config) {
  if (cursor.element_type() == ElementType::text) {
    translate_text(cursor, out, config);
  } else if (cursor.element_type() == ElementType::line_break) {
    translate_line_break(cursor, out, config);
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
  } else if (cursor.element_type() == ElementType::list_item) {
    translate_list_item(cursor, out, config);
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

void html::translate_slide(DocumentCursor &cursor, std::ostream &out,
                           const HtmlConfig &config) {
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
}

void html::translate_sheet(DocumentCursor &cursor, std::ostream &out,
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
  if (config.spreadsheet_limit) {
    end_row = config.spreadsheet_limit->rows;
    end_column = config.spreadsheet_limit->columns;
  }
  if (config.spreadsheet_limit_by_content) {
    const auto content = sheet.content(config.spreadsheet_limit);
    end_row = content.rows;
    end_column = content.columns;
  }

  auto shape_cursor = cursor;

  std::uint32_t column_index = 0;
  std::uint32_t row_index = 0;

  out << "<col>";

  column_index = 0;
  cursor.for_each_table_column([&](DocumentCursor &, const std::uint32_t) {
    auto table_column = cursor.element().table_column();

    out << "<col";
    out << optional_style_attribute(
        translate_table_column_style(table_column.style()));
    out << ">";

    ++column_index;
    return column_index < end_column;
  });

  {
    out << "<tr>";
    out << "<td style=\"width:30px;height:20px;\"/>";

    column_index = 0;
    cursor.for_each_table_column([&](DocumentCursor &, const std::uint32_t) {
      out << "<td style=\"text-align:center;vertical-align:middle;\">";
      out << common::TablePosition::to_column_string(column_index);
      out << "</td>";

      ++column_index;
      return column_index < end_column;
    });

    out << "</tr>";
  }

  cursor.for_each_table_row([&](DocumentCursor &cursor, const std::uint32_t) {
    auto table_row = cursor.element().table_row();
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

    column_index = 0;
    cursor.for_each_table_cell(
        [&](DocumentCursor &cursor, const std::uint32_t) {
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
            out << optional_style_attribute(
                translate_table_cell_style(table_cell.style()));
            if (table_cell.value_type() == ValueType::float_number) {
              out << " class=\"odr-value-type-float\"";
            }
            out << ">";
            if ((row_index == 0) && (column_index == 0)) {
              shape_cursor.for_each_sheet_shape(
                  [&](DocumentCursor &cursor, const std::uint32_t) {
                    translate_element(cursor, out, config);
                  });
            }
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

void html::translate_page(DocumentCursor &cursor, std::ostream &out,
                          const HtmlConfig &config) {
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
}

void html::translate_master_page(DocumentCursor &cursor, std::ostream &out,
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

void html::translate_text(const DocumentCursor &cursor, std::ostream &out,
                          const HtmlConfig &config) {
  auto text = cursor.element().text();

  out << "<x-s";
  out << optional_style_attribute(translate_text_style(text.style()));
  if (config.editable) {
    out << " contenteditable=\"true\"";
    out << " data-odr-path=\"" << cursor.document_path() << "\"";
  }
  out << ">";
  out << internal::html::escape_text(cursor.element().text().content());
  out << "</x-s>";
}

void html::translate_line_break(DocumentCursor &cursor, std::ostream &out,
                                const HtmlConfig &) {
  auto line_break = cursor.element().line_break();

  out << "<br>";
  out << "<x-s";
  out << optional_style_attribute(translate_text_style(line_break.style()));
  out << "></x-s>";
}

void html::translate_paragraph(DocumentCursor &cursor, std::ostream &out,
                               const HtmlConfig &config) {
  auto paragraph = cursor.element().paragraph();
  auto text_style_attribute =
      optional_style_attribute(translate_text_style(paragraph.text_style()));

  out << "<x-p";
  out << optional_style_attribute(translate_paragraph_style(paragraph.style()));
  out << ">";
  out << "<wbr>";
  translate_children(cursor, out, config);
  if (cursor.move_to_first_child()) {
    // TODO if element is content (e.g. bookmark does not count)

    // TODO example `encrypted-exception-3$aabbcc$.odt` at the very bottom
    // TODO has a missing line break after "As the result of the project we ..."

    // TODO example `style-missing+image-1.odt` first paragraph has no height

    cursor.move_to_parent();
  } else {
    out << "<x-s" << text_style_attribute << "></x-s>";
  }
  out << "</x-p>";
}

void html::translate_span(DocumentCursor &cursor, std::ostream &out,
                          const HtmlConfig &config) {
  auto span = cursor.element().span();

  out << "<x-s";
  out << optional_style_attribute(translate_text_style(span.style()));
  out << ">";
  translate_children(cursor, out, config);
  out << "</x-s>";
}

void html::translate_link(DocumentCursor &cursor, std::ostream &out,
                          const HtmlConfig &config) {
  auto link = cursor.element().link();

  out << "<a href=\"";
  out << link.href();
  out << "\">";
  translate_children(cursor, out, config);
  out << "</a>";
}

void html::translate_bookmark(DocumentCursor &cursor, std::ostream &out,
                              const HtmlConfig & /*config*/) {
  out << "<a id=\"";
  out << cursor.element().bookmark().name();
  out << "\"></a>";
}

void html::translate_list(DocumentCursor &cursor, std::ostream &out,
                          const HtmlConfig &config) {
  out << "<ul>";
  translate_children(cursor, out, config);
  out << "</ul>";
}

void html::translate_list_item(DocumentCursor &cursor, std::ostream &out,
                               const HtmlConfig &config) {
  auto list_item = cursor.element().list_item();
  out << "<li";
  out << optional_style_attribute(translate_text_style(list_item.style()));
  out << ">";
  translate_children(cursor, out, config);
  out << "</li>";
}

void html::translate_table(DocumentCursor &cursor, std::ostream &out,
                           const HtmlConfig &config) {
  // TODO table column width does not work
  // TODO table row height does not work
  auto table = cursor.element().table();

  out << "<table";
  out << optional_style_attribute(translate_table_style(table.style()));
  out << R"( cellpadding="0" border="0" cellspacing="0")";
  out << ">";

  cursor.for_each_table_column(
      [&](DocumentCursor &cursor, const std::uint32_t) {
        auto table_column = cursor.element().table_column();

        out << "<col";
        out << optional_style_attribute(
            translate_table_column_style(table_column.style()));
        out << ">";

        return true;
      });

  cursor.for_each_table_row([&](DocumentCursor &cursor, const std::uint32_t) {
    auto table_row = cursor.element().table_row();

    out << "<tr";
    out << optional_style_attribute(
        translate_table_row_style(table_row.style()));
    out << ">";

    cursor.for_each_table_cell(
        [&](DocumentCursor &cursor, const std::uint32_t) {
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
            out << optional_style_attribute(
                translate_table_cell_style(table_cell.style()));
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

void html::translate_image(DocumentCursor &cursor, std::ostream &out,
                           const HtmlConfig & /*config*/) {
  auto image = cursor.element().image();

  out << "<img style=\"position:absolute;left:0;top:0;width:100%;height:100%\"";
  out << " alt=\"Error: image not found or unsupported\"";
  out << " src=\"";

  if (image.internal()) {
    auto image_file = image.file().value();

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
    out << image.href();
  }

  out << "\">";
}

void html::translate_frame(DocumentCursor &cursor, std::ostream &out,
                           const HtmlConfig &config) {
  auto frame = cursor.element().frame();
  auto style = frame.style();

  out << "<div";

  out << optional_style_attribute(translate_frame_properties(frame) +
                                  translate_drawing_style(style));
  out << ">";
  translate_children(cursor, out, config);

  out << "</div>";
}

void html::translate_rect(DocumentCursor &cursor, std::ostream &out,
                          const HtmlConfig &config) {
  auto rect = cursor.element().rect();

  out << "<div";
  out << optional_style_attribute(
      translate_rect_properties(cursor.element().rect()) +
      translate_drawing_style(rect.style()));
  out << ">";
  translate_children(cursor, out, config);
  out << R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" overflow="visible" preserveAspectRatio="none" style="z-index:-1;width:inherit;height:inherit;position:absolute;top:0;left:0;padding:inherit;"><rect x="0" y="0" width="100%" height="100%" /></svg>)";
  out << "</div>";
}

void html::translate_line(DocumentCursor &cursor, std::ostream &out,
                          const HtmlConfig & /*config*/) {
  auto line = cursor.element().line();

  out << R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" overflow="visible")";
  out << optional_style_attribute("z-index:-1;position:absolute;top:0;left:0;" +
                                  translate_drawing_style(line.style()));
  out << ">";

  out << "<line";
  out << " x1=\"" << cursor.element().line().x1() << "\" y1=\""
      << cursor.element().line().y1() << "\"";
  out << " x2=\"" << cursor.element().line().x2() << "\" y2=\""
      << cursor.element().line().y2() << "\"";
  out << " />";

  out << "</svg>";
}

void html::translate_circle(DocumentCursor &cursor, std::ostream &out,
                            const HtmlConfig &config) {
  auto circle = cursor.element().circle();

  out << "<div";
  out << optional_style_attribute(
      translate_circle_properties(cursor.element().circle()) +
      translate_drawing_style(circle.style()));
  out << ">";
  translate_children(cursor, out, config);
  out << R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" overflow="visible" preserveAspectRatio="none" style="z-index:-1;width:inherit;height:inherit;position:absolute;top:0;left:0;padding:inherit;"><circle cx="50%" cy="50%" r="50%" /></svg>)";
  out << "</div>";
}

void html::translate_custom_shape(DocumentCursor &cursor, std::ostream &out,
                                  const HtmlConfig &config) {
  auto custom_shape = cursor.element().custom_shape();

  out << "<div";
  out << optional_style_attribute(
      translate_custom_shape_properties(cursor.element().custom_shape()) +
      translate_drawing_style(custom_shape.style()));
  out << ">";
  translate_children(cursor, out, config);
  // TODO draw shape in svg
  out << "</div>";
}

} // namespace odr::internal
