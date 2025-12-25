#include <odr/internal/html/document_element.hpp>

#include <odr/document_element.hpp>
#include <odr/document_path.hpp>
#include <odr/html.hpp>
#include <odr/style.hpp>

#include <odr/internal/common/table_cursor.hpp>
#include <odr/internal/html/common.hpp>
#include <odr/internal/html/document_style.hpp>
#include <odr/internal/html/html_service.hpp>
#include <odr/internal/html/html_writer.hpp>
#include <odr/internal/html/image_file.hpp>

namespace odr::internal {

void html::translate_children(const ElementRange &range,
                              const WritingState &state) {
  for (const Element child : range) {
    translate_element(child, state);
  }
}

void html::translate_element(const Element &element,
                             const WritingState &state) {
  if (element.type() == ElementType::text) {
    translate_text(element, state);
  } else if (element.type() == ElementType::line_break) {
    translate_line_break(element, state);
  } else if (element.type() == ElementType::paragraph) {
    translate_paragraph(element, state);
  } else if (element.type() == ElementType::span) {
    translate_span(element, state);
  } else if (element.type() == ElementType::link) {
    translate_link(element, state);
  } else if (element.type() == ElementType::bookmark) {
    translate_bookmark(element, state);
  } else if (element.type() == ElementType::list) {
    translate_list(element, state);
  } else if (element.type() == ElementType::list_item) {
    translate_list_item(element, state);
  } else if (element.type() == ElementType::table) {
    translate_table(element, state);
  } else if (element.type() == ElementType::frame) {
    translate_frame(element, state);
  } else if (element.type() == ElementType::image) {
    translate_image(element, state);
  } else if (element.type() == ElementType::rect) {
    translate_rect(element, state);
  } else if (element.type() == ElementType::line) {
    translate_line(element, state);
  } else if (element.type() == ElementType::circle) {
    translate_circle(element, state);
  } else if (element.type() == ElementType::custom_shape) {
    translate_custom_shape(element, state);
  } else if (element.type() == ElementType::group) {
    translate_children(element.children(), state);
  } else {
    // TODO log
  }
}

void html::translate_sheet(const Sheet &sheet, const WritingState &state) {
  state.out().write_element_begin(
      "table",
      HtmlElementOptions().set_attributes(HtmlAttributesVector{
          {"cellpadding", "0"}, {"border", "0"}, {"cellspacing", "0"}}));

  const TableDimensions dimensions = sheet.dimensions();
  std::uint32_t end_column = dimensions.columns;
  std::uint32_t end_row = dimensions.rows;
  if (state.config().spreadsheet_limit_by_content) {
    const TableDimensions content =
        sheet.content(state.config().spreadsheet_limit);
    end_column = content.columns;
    end_row = content.rows;
  }
  if (state.config().spreadsheet_limit) {
    end_column =
        std::min(end_column, state.config().spreadsheet_limit->columns);
    end_row = std::min(end_row, state.config().spreadsheet_limit->rows);
  }
  end_column = std::max(1u, end_column);
  end_row = std::max(1u, end_row);

  state.out().write_element_begin(
      "col", HtmlElementOptions().set_close_type(HtmlCloseType::none));

  for (std::uint32_t column_index = 0; column_index < end_column;
       ++column_index) {
    TableColumnStyle table_column_style = sheet.column_style(column_index);

    state.out().write_element_begin(
        "col",
        HtmlElementOptions()
            .set_close_type(HtmlCloseType::none)
            .set_style(translate_table_column_style(table_column_style)));
  }

  {
    state.out().write_element_begin("tr");

    state.out().write_element_begin("td",
                                    HtmlElementOptions()
                                        .set_close_type(HtmlCloseType::trailing)
                                        .set_style("width:30px;height:20px;"));

    for (std::uint32_t column_index = 0; column_index < end_column;
         ++column_index) {
      state.out().write_element_begin(
          "td", HtmlElementOptions().set_inline(true).set_style(
                    "text-align:center;vertical-align:middle;"));
      state.out().write_raw(TablePosition::to_column_string(column_index));
      state.out().write_element_end("td");
    }

    state.out().write_element_end("tr");
  }

  TableCursor cursor;
  for (std::uint32_t row_index = cursor.row(); row_index < end_row;
       row_index = cursor.row()) {
    TableRowStyle table_row_style = sheet.row_style(row_index);

    state.out().write_element_begin(
        "tr", HtmlElementOptions().set_style(
                  translate_table_row_style(table_row_style)));

    state.out().write_element_begin(
        "td", HtmlElementOptions().set_inline(true).set_style([&] {
          std::string style = "text-align:center;vertical-align:middle;";
          if (const std::optional<Measure> height = table_row_style.height) {
            style += "height:" + height->to_string() + ";";
            style += "max-height:" + height->to_string() + ";";
          }
          return style;
        }()));
    state.out().write_raw(TablePosition::to_row_string(row_index));
    state.out().write_element_end("td");

    for (std::uint32_t column_index = cursor.column();
         column_index < end_column; column_index = cursor.column()) {
      SheetCell cell = sheet.cell(column_index, row_index);

      if (cell.is_covered()) {
        continue;
      }

      // TODO looks a bit odd to query the same (col, row) all the time. maybe
      // there could be a struct to get all the info?
      TableCellStyle cell_style = cell.style();
      TableDimensions cell_span = cell.span();
      ValueType cell_value_type = cell.value_type();

      state.out().write_element_begin(
          "td",
          HtmlElementOptions()
              .set_attributes([&](const HtmlAttributeWriterCallback &clb) {
                if (cell_span.columns > 1) {
                  clb("colspan", std::to_string(cell_span.columns));
                }
                if (cell_span.rows > 1) {
                  clb("rowspan", std::to_string(cell_span.rows));
                }
              })
              .set_style(translate_table_cell_style(cell_style))
              .set_class([&]() -> std::optional<HtmlWritable> {
                if (cell_value_type == ValueType::float_number) {
                  return "odr-value-type-float";
                }
                return std::nullopt;
              }()));
      if (column_index == 0 && row_index == 0) {
        for (const Element shape : sheet.shapes()) {
          translate_element(shape, state);
        }
      }
      translate_children(cell.children(), state);
      state.out().write_element_end("td");

      cursor.add_cell(cell_span.columns, cell_span.rows);
    }

    state.out().write_element_end("tr");

    cursor.add_row();
  }

  state.out().write_element_end("table");
}

void html::translate_slide(const Slide &slide, const WritingState &state) {
  state.out().write_element_begin(
      "div", HtmlElementOptions()
                 .set_class("odr-page-outer")
                 .set_style(translate_outer_page_style(slide.page_layout())));
  //  state.out().write_element_begin(
  //      "div", HtmlElementOptions().set_class("odr-page-inner").set_style(
  //                 translate_inner_page_style(slide.page_layout())));

  translate_master_page(slide.master_page(), state);
  translate_children(slide.children(), state);

  //  state.out().write_element_end("div");
  state.out().write_element_end("div");
}

void html::translate_page(const Page &page, const WritingState &state) {
  state.out().write_element_begin(
      "div", HtmlElementOptions()
                 .set_class("odr-page-outer")
                 .set_style(translate_outer_page_style(page.page_layout())));
  //  state.out().write_element_begin(
  //      "div", HtmlElementOptions().set_class("odr-page-inner").set_style(
  //                 translate_inner_page_style(page.page_layout())));

  translate_master_page(page.master_page(), state);
  translate_children(page.children(), state);

  //  state.out().write_element_end("div");
  state.out().write_element_end("div");
}

void html::translate_master_page(const MasterPage &masterPage,
                                 const WritingState &state) {
  for (const Element child : masterPage.children()) {
    // TODO filter placeholders
    translate_element(child, state);
  }
}

void html::translate_text(const Element &element, const WritingState &state) {
  const Text text = element.as_text();

  state.out().write_element_begin(
      "x-s", HtmlElementOptions()
                 .set_inline(true)
                 .set_attributes([&](const HtmlAttributeWriterCallback &clb) {
                   if (state.config().editable && element.is_editable()) {
                     clb("contenteditable", "true");
                     clb("data-odr-path",
                         DocumentPath::extract(element).to_string());
                   }
                 })
                 .set_style(translate_text_style(text.style())));
  state.out().out() << escape_text(text.content());
  state.out().write_element_end("x-s");
}

void html::translate_line_break(const Element &element,
                                const WritingState &state) {
  const LineBreak line_break = element.as_line_break();

  state.out().write_element_begin(
      "br", HtmlElementOptions().set_close_type(HtmlCloseType::none));
  state.out().write_element_begin(
      "x-s", HtmlElementOptions().set_inline(true).set_style(
                 translate_text_style(line_break.style())));
  state.out().write_element_end("x-s");
}

void html::translate_paragraph(const Element &element,
                               const WritingState &state) {
  const Paragraph paragraph = element.as_paragraph();

  state.out().write_element_begin(
      "x-p",
      HtmlElementOptions().set_inline(true).set_style(
          "display:block;" + translate_paragraph_style(paragraph.style())));
  translate_children(paragraph.children(), state);
  if (paragraph.first_child()) {
    // TODO if element is content (e.g. bookmark does not count)

    // TODO example `encrypted-exception-3$aabbcc$.odt` at the very bottom
    // TODO has a missing line break after "As the result of the project we ..."

    // TODO example `style-missing+image-1.odt` first paragraph has no height
  } else {
    state.out().write_element_begin(
        "x-s", HtmlElementOptions().set_inline(true).set_style(
                   translate_text_style(paragraph.text_style())));
    state.out().write_element_end("x-s");
  }
  state.out().write_element_begin(
      "wbr", HtmlElementOptions().set_close_type(HtmlCloseType::none));
  state.out().write_element_end("x-p");
}

void html::translate_span(const Element &element, const WritingState &state) {
  const Span span = element.as_span();

  state.out().write_element_begin(
      "x-s", HtmlElementOptions().set_inline(true).set_style(
                 translate_text_style(span.style())));
  translate_children(span.children(), state);
  state.out().write_element_end("x-s");
}

void html::translate_link(const Element &element, const WritingState &state) {
  const Link link = element.as_link();

  state.out().write_element_begin(
      "a", HtmlElementOptions().set_inline(true).set_attributes(
               HtmlAttributesVector{{"href", link.href()}}));
  translate_children(link.children(), state);
  state.out().write_element_end("a");
}

void html::translate_bookmark(const Element &element,
                              const WritingState &state) {
  const Bookmark bookmark = element.as_bookmark();

  state.out().write_element_begin(
      "a", HtmlElementOptions().set_inline(true).set_attributes(
               HtmlAttributesVector{{"id", bookmark.name()}}));
  state.out().write_element_end("a");
}

void html::translate_list(const Element &element, const WritingState &state) {
  state.out().write_element_begin("ul");
  translate_children(element.children(), state);
  state.out().write_element_end("ul");
}

void html::translate_list_item(const Element &element,
                               const WritingState &state) {
  const ListItem list_item = element.as_list_item();

  state.out().write_element_begin(
      "li",
      HtmlElementOptions().set_style(translate_text_style(list_item.style())));
  translate_children(list_item.children(), state);
  state.out().write_element_end("li");
}

void html::translate_table(const Element &element, const WritingState &state) {
  const Table table = element.as_table();

  state.out().write_element_begin(
      "table",
      HtmlElementOptions()
          .set_attributes(HtmlAttributesVector{
              {"cellpadding", "0"}, {"border", "0"}, {"cellspacing", "0"}})
          .set_style(translate_table_style(table.style())));

  for (Element column : table.columns()) {
    TableColumn table_column = column.as_table_column();

    state.out().write_element_begin(
        "col",
        HtmlElementOptions()
            .set_close_type(HtmlCloseType::none)
            .set_style(translate_table_column_style(table_column.style())));
  }

  for (Element row : table.rows()) {
    TableRow table_row = row.as_table_row();

    state.out().write_element_begin(
        "tr", HtmlElementOptions().set_style(
                  translate_table_row_style(table_row.style())));

    for (Element cell : table_row.children()) {
      TableCell table_cell = cell.as_table_cell();

      if (table_cell.is_covered()) {
        continue;
      }

      TableDimensions cell_span = table_cell.span();

      state.out().write_element_begin(
          "td",
          HtmlElementOptions()
              .set_attributes([&](const HtmlAttributeWriterCallback &clb) {
                if (cell_span.columns > 1) {
                  clb("colspan", std::to_string(cell_span.columns));
                }
                if (cell_span.rows > 1) {
                  clb("rowspan", std::to_string(cell_span.rows));
                }
              })
              .set_style(translate_table_cell_style(table_cell.style())));

      translate_children(cell.children(), state);

      state.out().write_element_end("td");
    }

    state.out().write_element_end("tr");
  }

  state.out().write_element_end("table");
}

void html::translate_image(const Element &element, const WritingState &state) {
  const Image image = element.as_image();

  odr::HtmlResource resource;
  HtmlResourceLocation resource_location;
  if (image.is_internal()) {
    resource = HtmlResource::create(HtmlResourceType::image, "image/jpg",
                                    image.href(), image.href(),
                                    image.file().value(), false, false, true);
    resource_location =
        state.config().resource_locator(resource, state.config());
  } else {
    resource =
        HtmlResource::create(HtmlResourceType::image, "image/jpg", "image",
                             "image", std::nullopt, false, false, false);
    resource_location = image.href();
  }
  state.resources().emplace_back(std::move(resource), resource_location);

  state.out().write_element_begin(
      "img",
      HtmlElementOptions()
          .set_close_type(HtmlCloseType::trailing)
          .set_attributes([&](const HtmlAttributeWriterCallback &clb) {
            clb("alt", "Error: image not found or unsupported");
            if (resource_location.has_value()) {
              clb("src", resource_location.value());
            } else {
              clb("src", [&](std::ostream &o) {
                translate_image_src(image.file().value(), o, state.config());
              });
            }
          })
          .set_style("position:absolute;left:0;top:0;width:100%;height:100%"));
}

void html::translate_frame(const Element &element, const WritingState &state) {
  const Frame frame = element.as_frame();
  const GraphicStyle style = frame.style();

  state.out().write_element_begin(
      "div", HtmlElementOptions().set_style(translate_frame_properties(frame) +
                                            translate_drawing_style(style)));
  translate_children(frame.children(), state);
  state.out().write_element_end("div");
}

void html::translate_rect(const Element &element, const WritingState &state) {
  const Rect rect = element.as_rect();
  const GraphicStyle style = rect.style();

  state.out().write_element_begin(
      "div", HtmlElementOptions().set_style(translate_rect_properties(rect) +
                                            translate_drawing_style(style)));
  translate_children(rect.children(), state);
  state.out().write_new_line();
  state.out().write_raw(
      R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" overflow="visible" preserveAspectRatio="none" style="z-index:-1;width:inherit;height:inherit;position:absolute;top:0;left:0;padding:inherit;"><rect x="0" y="0" width="100%" height="100%" /></svg>)");
  state.out().write_element_end("div");
}

void html::translate_line(const Element &element, const WritingState &state) {
  const Line line = element.as_line();
  const GraphicStyle style = line.style();

  state.out().write_element_begin(
      "svg", HtmlElementOptions()
                 .set_attributes(HtmlAttributesVector{
                     {"xmlns", "http://www.w3.org/2000/svg"},
                     {"version", "1.1"},
                     {"overflow", "visible"}})
                 .set_style("z-index:-1;position:absolute;top:0;left:0;" +
                            translate_drawing_style(style)));

  state.out().write_element_begin(
      "line", HtmlElementOptions()
                  .set_close_type(HtmlCloseType::trailing)
                  .set_attributes(HtmlAttributesVector{{"x1", line.x1()},
                                                       {"y1", line.y1()},
                                                       {"x2", line.x2()},
                                                       {"y2", line.y2()}}));

  state.out().write_element_end("svg");
}

void html::translate_circle(const Element &element, const WritingState &state) {
  const Circle circle = element.as_circle();
  const GraphicStyle style = circle.style();

  state.out().write_element_begin(
      "div",
      HtmlElementOptions().set_style(translate_circle_properties(circle) +
                                     translate_drawing_style(style)));
  state.out().write_new_line();
  translate_children(circle.children(), state);
  state.out().write_raw(
      R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" overflow="visible" preserveAspectRatio="none" style="z-index:-1;width:inherit;height:inherit;position:absolute;top:0;left:0;padding:inherit;"><circle cx="50%" cy="50%" r="50%" /></svg>)");
  state.out().write_element_end("div");
}

void html::translate_custom_shape(const Element &element,
                                  const WritingState &state) {
  const CustomShape custom_shape = element.as_custom_shape();
  const GraphicStyle style = custom_shape.style();

  state.out().write_element_begin(
      "div", HtmlElementOptions().set_style(
                 translate_custom_shape_properties(custom_shape) +
                 translate_drawing_style(style)));
  translate_children(custom_shape.children(), state);
  // TODO draw shape in svg
  state.out().write_element_end("div");
}

} // namespace odr::internal
