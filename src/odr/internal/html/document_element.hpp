#pragma once

#include <odr/html.hpp>

#include <string>

namespace odr {
class Element;
class ElementRange;
class MasterPage;
class Slide;
class Page;
} // namespace odr

namespace odr::internal::html {
struct WritingState;
class HtmlWriter;

void translate_children(const ElementRange &range, const WritingState &state);
void translate_element(const Element &element, const WritingState &state);

/// Whether a reader sees anything. A bookmark marks a place rather than
/// filling one, and a span or a link is a style around what it holds, so a
/// paragraph holding only those is still an empty line.
[[nodiscard]] bool has_content(const ElementRange &children);
/// A break where the paragraph holds nothing, so a blank line survives being
/// pasted elsewhere; otherwise a break opportunity, or content all out of
/// flow leaves no line box.
void write_paragraph_line_box(bool empty, const WritingState &state);
/// Whether @p element carries the address an edit operation names.
/// `contenteditable` is not written: the mode adds it to these runs.
[[nodiscard]] bool writes_edit_markup(const Element &element,
                                      const WritingState &state);

void translate_slide(const Slide &slide, const WritingState &state);
void translate_page(const Page &page, const WritingState &state);

void translate_master_page(const MasterPage &masterPage,
                           const WritingState &state);

void translate_text(const Element &element, const WritingState &state);
void translate_line_break(const Element &element, const WritingState &state);
/// A manual page break carried as an element, which only rtf and `.doc` do.
void translate_page_break(const Element &element, const WritingState &state);
/// `marker`, when set, is written inside the paragraph ahead of its content.
void translate_paragraph(const Element &element, const WritingState &state,
                         const std::string &marker = "");
void translate_span(const Element &element, const WritingState &state);
void translate_link(const Element &element, const WritingState &state);
void translate_bookmark(const Element &element, const WritingState &state);
void translate_list(const Element &element, const WritingState &state);
void translate_list_item(const Element &element, const WritingState &state);
void translate_table(const Element &element, const WritingState &state);
void translate_image(const Element &element, const WritingState &state);
/// Writes the box, and the svg outline of whatever shape it draws.
void translate_frame(const Element &element, const WritingState &state);

} // namespace odr::internal::html
