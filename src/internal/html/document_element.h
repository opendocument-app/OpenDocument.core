#ifndef ODR_INTERNAL_HTML_DOCUMENT_ELEMENT_H
#define ODR_INTERNAL_HTML_DOCUMENT_ELEMENT_H

#include <iosfwd>

namespace odr {
class DocumentCursor;

struct HtmlConfig;
} // namespace odr

namespace odr::internal::html {

void translate_children(DocumentCursor &cursor, std::ostream &out,
                        const HtmlConfig &config);
void translate_element(DocumentCursor &cursor, std::ostream &out,
                       const HtmlConfig &config);

void translate_slide(DocumentCursor &cursor, std::ostream &out,
                     const HtmlConfig &config);
void translate_sheet(DocumentCursor &cursor, std::ostream &out,
                     const HtmlConfig &config);
void translate_page(DocumentCursor &cursor, std::ostream &out,
                    const HtmlConfig &config);

void translate_master_page(DocumentCursor &cursor, std::ostream &out,
                           const HtmlConfig &config);

void translate_text(const DocumentCursor &cursor, std::ostream &out,
                    const HtmlConfig &config);
void translate_line_break(DocumentCursor &cursor, std::ostream &out,
                          const HtmlConfig &);
void translate_paragraph(DocumentCursor &cursor, std::ostream &out,
                         const HtmlConfig &config);
void translate_span(DocumentCursor &cursor, std::ostream &out,
                    const HtmlConfig &config);
void translate_link(DocumentCursor &cursor, std::ostream &out,
                    const HtmlConfig &config);
void translate_bookmark(DocumentCursor &cursor, std::ostream &out,
                        const HtmlConfig &config);
void translate_list(DocumentCursor &cursor, std::ostream &out,
                    const HtmlConfig &config);
void translate_list_item(DocumentCursor &cursor, std::ostream &out,
                         const HtmlConfig &config);
void translate_table(DocumentCursor &cursor, std::ostream &out,
                     const HtmlConfig &config);
void translate_image(DocumentCursor &cursor, std::ostream &out,
                     const HtmlConfig &config);
void translate_frame(DocumentCursor &cursor, std::ostream &out,
                     const HtmlConfig &config);
void translate_rect(DocumentCursor &cursor, std::ostream &out,
                    const HtmlConfig &config);
void translate_line(DocumentCursor &cursor, std::ostream &out,
                    const HtmlConfig &config);
void translate_circle(DocumentCursor &cursor, std::ostream &out,
                      const HtmlConfig &config);
void translate_custom_shape(DocumentCursor &cursor, std::ostream &out,
                            const HtmlConfig &config);

} // namespace odr::internal::html

#endif // ODR_INTERNAL_HTML_DOCUMENT_ELEMENT_H
