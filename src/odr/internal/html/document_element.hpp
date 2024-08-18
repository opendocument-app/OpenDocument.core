#ifndef ODR_INTERNAL_HTML_DOCUMENT_ELEMENT_HPP
#define ODR_INTERNAL_HTML_DOCUMENT_ELEMENT_HPP

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
class HtmlWriter;

void translate_children(ElementRange range, HtmlWriter &out,
                        const HtmlConfig &config,
                        const HtmlResourceLocator &resourceLocator);
void translate_element(Element element, HtmlWriter &out,
                       const HtmlConfig &config,
                       const HtmlResourceLocator &resourceLocator);

void translate_slide(Slide slide, HtmlWriter &out, const HtmlConfig &config,
                     const HtmlResourceLocator &resourceLocator);
void translate_sheet(Sheet sheet, HtmlWriter &out, const HtmlConfig &config,
                     const HtmlResourceLocator &resourceLocator);
void translate_page(Page page, HtmlWriter &out, const HtmlConfig &config,
                    const HtmlResourceLocator &resourceLocator);

void translate_master_page(MasterPage masterPage, HtmlWriter &out,
                           const HtmlConfig &config,
                           const HtmlResourceLocator &resourceLocator);

void translate_text(Element element, HtmlWriter &out, const HtmlConfig &config,
                    const HtmlResourceLocator &resourceLocator);
void translate_line_break(Element element, HtmlWriter &out,
                          const HtmlConfig &config,
                          const HtmlResourceLocator &resourceLocator);
void translate_paragraph(Element element, HtmlWriter &out,
                         const HtmlConfig &config,
                         const HtmlResourceLocator &resourceLocator);
void translate_span(Element element, HtmlWriter &out, const HtmlConfig &config,
                    const HtmlResourceLocator &resourceLocator);
void translate_link(Element element, HtmlWriter &out, const HtmlConfig &config,
                    const HtmlResourceLocator &resourceLocator);
void translate_bookmark(Element element, HtmlWriter &out,
                        const HtmlConfig &config,
                        const HtmlResourceLocator &resourceLocator);
void translate_list(Element element, HtmlWriter &out, const HtmlConfig &config,
                    const HtmlResourceLocator &resourceLocator);
void translate_list_item(Element element, HtmlWriter &out,
                         const HtmlConfig &config,
                         const HtmlResourceLocator &resourceLocator);
void translate_table(Element element, HtmlWriter &out, const HtmlConfig &config,
                     const HtmlResourceLocator &resourceLocator);
void translate_image(Element element, HtmlWriter &out, const HtmlConfig &config,
                     const HtmlResourceLocator &resourceLocator);
void translate_frame(Element element, HtmlWriter &out, const HtmlConfig &config,
                     const HtmlResourceLocator &resourceLocator);
void translate_rect(Element element, HtmlWriter &out, const HtmlConfig &config,
                    const HtmlResourceLocator &resourceLocator);
void translate_line(Element element, HtmlWriter &out, const HtmlConfig &config,
                    const HtmlResourceLocator &resourceLocator);
void translate_circle(Element element, HtmlWriter &out,
                      const HtmlConfig &config,
                      const HtmlResourceLocator &resourceLocator);
void translate_custom_shape(Element element, HtmlWriter &out,
                            const HtmlConfig &config,
                            const HtmlResourceLocator &resourceLocator);

} // namespace odr::internal::html

#endif // ODR_INTERNAL_HTML_DOCUMENT_ELEMENT_HPP
