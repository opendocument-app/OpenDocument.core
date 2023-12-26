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
                              const HtmlConfig &config) {}

void html::translate_element(Element element, HtmlWriter &out,
                             const HtmlConfig &config) {}

void html::translate_sheet(Element element, HtmlWriter &out,
                           const HtmlConfig &config) {}

void html::translate_slide(Element element, HtmlWriter &out,
                           const HtmlConfig &config) {}

void html::translate_page(Element element, HtmlWriter &out,
                          const HtmlConfig &config) {}

void html::translate_master_page(MasterPage element, HtmlWriter &out,
                                 const HtmlConfig &config) {}

void html::translate_text(const Element element, HtmlWriter &out,
                          const HtmlConfig &config) {}

void html::translate_line_break(Element element, HtmlWriter &out,
                                const HtmlConfig &) {}

void html::translate_paragraph(Element element, HtmlWriter &out,
                               const HtmlConfig &config) {}

void html::translate_span(Element element, HtmlWriter &out,
                          const HtmlConfig &config) {}

void html::translate_link(Element element, HtmlWriter &out,
                          const HtmlConfig &config) {}

void html::translate_bookmark(Element element, HtmlWriter &out,
                              const HtmlConfig & /*config*/) {}

void html::translate_list(Element element, HtmlWriter &out,
                          const HtmlConfig &config) {}

void html::translate_list_item(Element element, HtmlWriter &out,
                               const HtmlConfig &config) {}

void html::translate_table(Element element, HtmlWriter &out,
                           const HtmlConfig &config) {}

void html::translate_image(Element element, HtmlWriter &out,
                           const HtmlConfig &config) {}

void html::translate_frame(Element element, HtmlWriter &out,
                           const HtmlConfig &config) {}

void html::translate_rect(Element element, HtmlWriter &out,
                          const HtmlConfig &config) {}

void html::translate_line(Element element, HtmlWriter &out,
                          const HtmlConfig & /*config*/) {}

void html::translate_circle(Element element, HtmlWriter &out,
                            const HtmlConfig &config) {}

void html::translate_custom_shape(Element element, HtmlWriter &out,
                                  const HtmlConfig &config) {}

} // namespace odr::internal
