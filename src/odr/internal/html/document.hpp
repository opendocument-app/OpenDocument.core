#ifndef ODR_INTERNAL_HTML_DOCUMENT_H
#define ODR_INTERNAL_HTML_DOCUMENT_H

#include <string>

namespace odr {
class Document;

struct HtmlConfig;
class Html;
} // namespace odr

namespace odr::internal::html {

Html translate_document(const Document &document,
                        const std::string &output_path,
                        const HtmlConfig &config);

Html translate_text_document(const Document &document,
                             const std::string &output_path,
                             const HtmlConfig &config);
Html translate_presentation(const Document &document,
                            const std::string &output_path,
                            const HtmlConfig &config);
Html translate_spreadsheet(const Document &document,
                           const std::string &output_path,
                           const HtmlConfig &config);
Html translate_drawing(const Document &document, const std::string &output_path,
                       const HtmlConfig &config);

} // namespace odr::internal::html

#endif // ODR_INTERNAL_HTML_DOCUMENT_H
