#include <odr/internal/html/document_element.hpp>

#include <odr/document_element.hpp>
#include <odr/document_path.hpp>
#include <odr/html.hpp>

#include <odr/internal/html/common.hpp>
#include <odr/internal/html/document_style.hpp>
#include <odr/internal/html/html_writer.hpp>
#include <odr/internal/html/image_file.hpp>

namespace odr::internal {

void html::translate_master_page(MasterPage element, HtmlWriter &out,
                                 const HtmlConfig &config) {
  for (Element child : element.children()) {
    // TODO filter placeholders
    translate_element(child, out, config);
  }
}

void html::translate_text(const Element element, HtmlWriter &out,
                          const HtmlConfig &config) {
  Text text = element.text();

  out.write_element_begin(
      "x-s", {.inline_element = true,
              .attributes =
                  [&](const HtmlAttributeWriterCallback &clb) {
                    if (config.editable && element.is_editable()) {
                      clb("contenteditable", "true");
                      clb("data-odr-path",
                          DocumentPath::extract(element).to_string());
                    }
                  },
              .style = translate_text_style(text.style())});
  out.out() << internal::html::escape_text(text.content());
  out.write_element_end("x-s");
}

void html::translate_line_break(Element element, HtmlWriter &out,
                                const HtmlConfig &) {
  LineBreak line_break = element.line_break();

  out.write_element_begin("br", {.close_type = HtmlCloseType::none});
  out.write_element_begin("x-s",
                          {.inline_element = true,
                           .style = translate_text_style(line_break.style())});
  out.write_element_end("x-s");
}

void html::translate_paragraph(Element element, HtmlWriter &out,
                               const HtmlConfig &config) {
  Paragraph paragraph = element.paragraph();

  out.write_element_begin(
      "x-p", {.inline_element = true,
              .style = "display:block;" +
                       translate_paragraph_style(paragraph.style())});
  translate_children(paragraph.children(), out, config);
  if (paragraph.first_child()) {
    // TODO if element is content (e.g. bookmark does not count)

    // TODO example `encrypted-exception-3$aabbcc$.odt` at the very bottom
    // TODO has a missing line break after "As the result of the project we ..."

    // TODO example `style-missing+image-1.odt` first paragraph has no height
  } else {
    out.write_element_begin(
        "x-s", {.inline_element = true,
                .style = translate_text_style(paragraph.text_style())});
    out.write_element_end("x-s");
  }
  out.write_element_begin("wbr", {.close_type = HtmlCloseType::none});
  out.write_element_end("x-p");
}

void html::translate_span(Element element, HtmlWriter &out,
                          const HtmlConfig &config) {
  Span span = element.span();

  out.write_element_begin("x-s", {.inline_element = true,
                                  .style = translate_text_style(span.style())});
  translate_children(span.children(), out, config);
  out.write_element_end("x-s");
}

void html::translate_link(Element element, HtmlWriter &out,
                          const HtmlConfig &config) {
  Link link = element.link();

  out.write_element_begin(
      "a", {.inline_element = true,
            .attributes = HtmlAttributesVector{{"href", link.href()}}});
  translate_children(link.children(), out, config);
  out.write_element_end("a");
}

void html::translate_bookmark(Element element, HtmlWriter &out,
                              const HtmlConfig & /*config*/) {
  Bookmark bookmark = element.bookmark();

  out.write_element_begin(
      "a", {.inline_element = true,
            .attributes = HtmlAttributesVector{{"id", bookmark.name()}}});
  out.write_element_end("a");
}

void html::translate_list(Element element, HtmlWriter &out,
                          const HtmlConfig &config) {
  out.write_element_begin("ul");
  translate_children(element.children(), out, config);
  out.write_element_end("ul");
}

void html::translate_list_item(Element element, HtmlWriter &out,
                               const HtmlConfig &config) {
  ListItem list_item = element.list_item();

  out.write_element_begin("li",
                          {.style = translate_text_style(list_item.style())});
  translate_children(list_item.children(), out, config);
  out.write_element_end("li");
}

void html::translate_table(Element element, HtmlWriter &out,
                           const HtmlConfig &config) {
  Table table = element.table();

  out.write_element_begin(
      "table", {.attributes = HtmlAttributesVector{{"cellpadding", "0"},
                                                   {"border", "0"},
                                                   {"cellspacing", "0"}},
                .style = translate_table_style(table.style())});

  for (Element column : table.columns()) {
    TableColumn table_column = column.table_column();

    out.write_element_begin(
        "col", {.close_type = HtmlCloseType::none,
                .style = translate_table_column_style(table_column.style())});
  }

  for (Element row : table.rows()) {
    TableRow table_row = row.table_row();

    out.write_element_begin(
        "tr", {.style = translate_table_row_style(table_row.style())});

    for (Element cell : table_row.children()) {
      TableCell table_cell = cell.table_cell();

      if (!table_cell.is_covered()) {
        TableDimensions cell_span = table_cell.span();

        out.write_element_begin(
            "td", {.attributes =
                       [&](const HtmlAttributeWriterCallback &clb) {
                         if (cell_span.columns > 1) {
                           clb("colspan", std::to_string(cell_span.columns));
                         }
                         if (cell_span.rows > 1) {
                           clb("rowspan", std::to_string(cell_span.rows));
                         }
                       },
                   .style = translate_table_cell_style(table_cell.style())});

        translate_children(cell.children(), out, config);

        out.write_element_end("td");
      }
    }

    out.write_element_end("tr");
  }

  out.write_element_end("table");
}

void html::translate_image(Element element, HtmlWriter &out,
                           const HtmlConfig &config) {
  Image image = element.image();

  out.write_element_begin(
      "img",
      {.close_type = HtmlCloseType::trailing,
       .attributes =
           [&](const HtmlAttributeWriterCallback &clb) {
             clb("alt", "Error: image not found or unsupported");
             if (image.is_internal()) {
               clb("src", [&](std::ostream &o) {
                 translate_image_src(image.file().value(), o, config);
               });
             } else {
               clb("src", image.href());
             }
           },
       .style = "position:absolute;left:0;top:0;width:100%;height:100%"});
}

void html::translate_frame(Element element, HtmlWriter &out,
                           const HtmlConfig &config) {
  Frame frame = element.frame();
  GraphicStyle style = frame.style();

  out.write_element_begin("div", {.style = translate_frame_properties(frame) +
                                           translate_drawing_style(style)});
  translate_children(frame.children(), out, config);
  out.write_element_end("div");
}

void html::translate_rect(Element element, HtmlWriter &out,
                          const HtmlConfig &config) {
  Rect rect = element.rect();
  GraphicStyle style = rect.style();

  out.write_element_begin("div", {.style = translate_rect_properties(rect) +
                                           translate_drawing_style(style)});
  translate_children(rect.children(), out, config);
  out.write_new_line();
  out.write_raw(
      R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" overflow="visible" preserveAspectRatio="none" style="z-index:-1;width:inherit;height:inherit;position:absolute;top:0;left:0;padding:inherit;"><rect x="0" y="0" width="100%" height="100%" /></svg>)");
  out.write_element_end("div");
}

void html::translate_line(Element element, HtmlWriter &out,
                          const HtmlConfig & /*config*/) {
  Line line = element.line();
  GraphicStyle style = line.style();

  out.write_element_begin(
      "svg", {.attributes =
                  HtmlAttributesVector{{"xmlns", "http://www.w3.org/2000/svg"},
                                       {"version", "1.1"},
                                       {"overflow", "visible"}},
              .style = "z-index:-1;position:absolute;top:0;left:0;" +
                       translate_drawing_style(style)});

  out.write_element_begin(
      "line", {.close_type = HtmlCloseType::trailing,
               .attributes = HtmlAttributesVector{{"x1", line.x1()},
                                                  {"y1", line.y1()},
                                                  {"x2", line.x2()},
                                                  {"y2", line.y2()}}});

  out.write_element_end("svg");
}

void html::translate_circle(Element element, HtmlWriter &out,
                            const HtmlConfig &config) {
  Circle circle = element.circle();
  GraphicStyle style = circle.style();

  out.write_element_begin("div", {.style = translate_circle_properties(circle) +
                                           translate_drawing_style(style)});
  out.write_new_line();
  translate_children(circle.children(), out, config);
  out.write_raw(
      R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" overflow="visible" preserveAspectRatio="none" style="z-index:-1;width:inherit;height:inherit;position:absolute;top:0;left:0;padding:inherit;"><circle cx="50%" cy="50%" r="50%" /></svg>)");
  out.write_element_end("div");
}

void html::translate_custom_shape(Element element, HtmlWriter &out,
                                  const HtmlConfig &config) {
  CustomShape custom_shape = element.custom_shape();
  GraphicStyle style = custom_shape.style();

  out.write_element_begin(
      "div", {.style = translate_custom_shape_properties(custom_shape) +
                       translate_drawing_style(style)});
  translate_children(custom_shape.children(), out, config);
  // TODO draw shape in svg
  out.write_element_end("div");
}

} // namespace odr::internal
