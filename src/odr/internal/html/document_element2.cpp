#include <odr/internal/html/document_element.hpp>

#include <odr/document_element.hpp>
#include <odr/document_path.hpp>
#include <odr/html.hpp>

#include <odr/internal/common/table_cursor.hpp>
#include <odr/internal/html/common.hpp>
#include <odr/internal/html/document_style.hpp>
#include <odr/internal/html/html_writer.hpp>

namespace odr::internal {

void html::translate_children(ElementRange range, HtmlWriter &out,
                              const HtmlConfig &config) {
  for (Element child : range) {
    translate_element(child, out, config);
  }
}

void html::translate_element(Element element, HtmlWriter &out,
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

void html::translate_slide(Element element, HtmlWriter &out,
                           const HtmlConfig &config) {
  Slide slide = element.slide();

  out.write_element_begin(
      "div", {.style = translate_outer_page_style(slide.page_layout())});
  out.write_element_begin(
      "div", {.style = translate_inner_page_style(slide.page_layout())});

  translate_master_page(slide.master_page(), out, config);
  translate_children(slide.children(), out, config);

  out.write_element_end("div");
  out.write_element_end("div");
}

void html::translate_page(Element element, HtmlWriter &out,
                          const HtmlConfig &config) {
  Page page = element.page();

  out.write_element_begin(
      "div", {.style = translate_outer_page_style(page.page_layout())});
  out.write_element_begin(
      "div", {.style = translate_inner_page_style(page.page_layout())});
  translate_master_page(page.master_page(), out, config);
  translate_children(page.children(), out, config);
  out.write_element_end("div");
  out.write_element_end("div");
}

} // namespace odr::internal
