#pragma once

#include <odr/html_service.hpp>

namespace odr {
class Element;
class ElementRange;
class MasterPage;
class Slide;
class Sheet;
class Page;
} // namespace odr

namespace odr::internal::html {
struct WritingState;
class HtmlWriter;

void translate_children(ElementRange range, const WritingState &state);
void translate_element(Element element, const WritingState &state);

void translate_slide(Slide slide, const WritingState &state);
void translate_sheet(Sheet sheet, const WritingState &state);
void translate_page(Page page, const WritingState &state);

void translate_master_page(MasterPage masterPage, const WritingState &state);

void translate_text(Element element, const WritingState &state);
void translate_line_break(Element element, const WritingState &state);
void translate_paragraph(Element element, const WritingState &state);
void translate_span(Element element, const WritingState &state);
void translate_link(Element element, const WritingState &state);
void translate_bookmark(Element element, const WritingState &state);
void translate_list(Element element, const WritingState &state);
void translate_list_item(Element element, const WritingState &state);
void translate_table(Element element, const WritingState &state);
void translate_image(Element element, const WritingState &state);
void translate_frame(Element element, const WritingState &state);
void translate_rect(Element element, const WritingState &state);
void translate_line(Element element, const WritingState &state);
void translate_circle(Element element, const WritingState &state);
void translate_custom_shape(Element element, const WritingState &state);

} // namespace odr::internal::html
