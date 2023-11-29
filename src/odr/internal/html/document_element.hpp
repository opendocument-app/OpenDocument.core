#ifndef ODR_INTERNAL_HTML_DOCUMENT_ELEMENT_H
#define ODR_INTERNAL_HTML_DOCUMENT_ELEMENT_H

#include <iosfwd>

namespace odr {
class Element;

struct HtmlConfig;
} // namespace odr

namespace odr::internal::html {

void translate_children(ElementRange range, std::ostream &out,
                        const HtmlConfig &config);
void translate_element(Element element, std::ostream &out,
                       const HtmlConfig &config);

void translate_slide(Element element, std::ostream &out,
                     const HtmlConfig &config);
void translate_sheet(Element element, std::ostream &out,
                     const HtmlConfig &config);
void translate_page(Element element, std::ostream &out,
                    const HtmlConfig &config);

void translate_master_page(MasterPage element, std::ostream &out,
                           const HtmlConfig &config);

void translate_text(const Element element, std::ostream &out,
                    const HtmlConfig &config);
void translate_line_break(Element element, std::ostream &out,
                          const HtmlConfig &);
void translate_paragraph(Element element, std::ostream &out,
                         const HtmlConfig &config);
void translate_span(Element element, std::ostream &out,
                    const HtmlConfig &config);
void translate_link(Element element, std::ostream &out,
                    const HtmlConfig &config);
void translate_bookmark(Element element, std::ostream &out,
                        const HtmlConfig &config);
void translate_list(Element element, std::ostream &out,
                    const HtmlConfig &config);
void translate_list_item(Element element, std::ostream &out,
                         const HtmlConfig &config);
void translate_table(Element element, std::ostream &out,
                     const HtmlConfig &config);
void translate_image(Element element, std::ostream &out,
                     const HtmlConfig &config);
void translate_frame(Element element, std::ostream &out,
                     const HtmlConfig &config);
void translate_rect(Element element, std::ostream &out,
                    const HtmlConfig &config);
void translate_line(Element element, std::ostream &out,
                    const HtmlConfig &config);
void translate_circle(Element element, std::ostream &out,
                      const HtmlConfig &config);
void translate_custom_shape(Element element, std::ostream &out,
                            const HtmlConfig &config);

} // namespace odr::internal::html

#endif // ODR_INTERNAL_HTML_DOCUMENT_ELEMENT_H
