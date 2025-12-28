#pragma once

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

void translate_children(const ElementRange &range, const WritingState &state);
void translate_element(const Element &element, const WritingState &state);

void translate_slide(const Slide &slide, const WritingState &state);
void translate_sheet(const Sheet &sheet, const WritingState &state);
void translate_page(const Page &page, const WritingState &state);

void translate_master_page(const MasterPage &masterPage,
                           const WritingState &state);

void translate_text(const Element &element, const WritingState &state);
void translate_line_break(const Element &element, const WritingState &state);
void translate_paragraph(const Element &element, const WritingState &state);
void translate_span(const Element &element, const WritingState &state);
void translate_link(const Element &element, const WritingState &state);
void translate_bookmark(const Element &element, const WritingState &state);
void translate_list(const Element &element, const WritingState &state);
void translate_list_item(const Element &element, const WritingState &state);
void translate_table(const Element &element, const WritingState &state);
void translate_image(const Element &element, const WritingState &state);
void translate_frame(const Element &element, const WritingState &state);
void translate_rect(const Element &element, const WritingState &state);
void translate_line(const Element &element, const WritingState &state);
void translate_circle(const Element &element, const WritingState &state);
void translate_custom_shape(const Element &element, const WritingState &state);

} // namespace odr::internal::html
