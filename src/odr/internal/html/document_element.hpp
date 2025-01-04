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
class HtmlWriter;

void translate_children(ElementRange range, HtmlWriter &out,
                        const HtmlConfig &config,
                        const HtmlResourceLocator &resource_locator,
                        HtmlResources &resources);
void translate_element(Element element, HtmlWriter &out,
                       const HtmlConfig &config,
                       const HtmlResourceLocator &resource_locator,
                       HtmlResources &resources);

void translate_slide(Slide slide, HtmlWriter &out, const HtmlConfig &config,
                     const HtmlResourceLocator &resource_locator,
                     HtmlResources &resources);
void translate_sheet(Sheet sheet, HtmlWriter &out, const HtmlConfig &config,
                     const HtmlResourceLocator &resource_locator,
                     HtmlResources &resources);
void translate_page(Page page, HtmlWriter &out, const HtmlConfig &config,
                    const HtmlResourceLocator &resource_locator,
                    HtmlResources &resources);

void translate_master_page(MasterPage masterPage, HtmlWriter &out,
                           const HtmlConfig &config,
                           const HtmlResourceLocator &resource_locator,
                           HtmlResources &resources);

void translate_text(Element element, HtmlWriter &out, const HtmlConfig &config,
                    const HtmlResourceLocator &resource_locator,
                    HtmlResources &resources);
void translate_line_break(Element element, HtmlWriter &out,
                          const HtmlConfig &config,
                          const HtmlResourceLocator &resource_locator,
                          HtmlResources &resources);
void translate_paragraph(Element element, HtmlWriter &out,
                         const HtmlConfig &config,
                         const HtmlResourceLocator &resource_locator,
                         HtmlResources &resources);
void translate_span(Element element, HtmlWriter &out, const HtmlConfig &config,
                    const HtmlResourceLocator &resource_locator,
                    HtmlResources &resources);
void translate_link(Element element, HtmlWriter &out, const HtmlConfig &config,
                    const HtmlResourceLocator &resource_locator,
                    HtmlResources &resources);
void translate_bookmark(Element element, HtmlWriter &out,
                        const HtmlConfig &config,
                        const HtmlResourceLocator &resource_locator,
                        HtmlResources &resources);
void translate_list(Element element, HtmlWriter &out, const HtmlConfig &config,
                    const HtmlResourceLocator &resource_locator,
                    HtmlResources &resources);
void translate_list_item(Element element, HtmlWriter &out,
                         const HtmlConfig &config,
                         const HtmlResourceLocator &resource_locator,
                         HtmlResources &resources);
void translate_table(Element element, HtmlWriter &out, const HtmlConfig &config,
                     const HtmlResourceLocator &resource_locator,
                     HtmlResources &resources);
void translate_image(Element element, HtmlWriter &out, const HtmlConfig &config,
                     const HtmlResourceLocator &resource_locator,
                     HtmlResources &resources);
void translate_frame(Element element, HtmlWriter &out, const HtmlConfig &config,
                     const HtmlResourceLocator &resource_locator,
                     HtmlResources &resources);
void translate_rect(Element element, HtmlWriter &out, const HtmlConfig &config,
                    const HtmlResourceLocator &resource_locator,
                    HtmlResources &resources);
void translate_line(Element element, HtmlWriter &out, const HtmlConfig &config,
                    const HtmlResourceLocator &resource_locator,
                    HtmlResources &resources);
void translate_circle(Element element, HtmlWriter &out,
                      const HtmlConfig &config,
                      const HtmlResourceLocator &resource_locator,
                      HtmlResources &resources);
void translate_custom_shape(Element element, HtmlWriter &out,
                            const HtmlConfig &config,
                            const HtmlResourceLocator &resource_locator,
                            HtmlResources &resources);

} // namespace odr::internal::html
