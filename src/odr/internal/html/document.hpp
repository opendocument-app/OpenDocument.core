#ifndef ODR_INTERNAL_HTML_DOCUMENT_HPP
#define ODR_INTERNAL_HTML_DOCUMENT_HPP

#include <string>

namespace odr {
class Document;
struct HtmlConfig;
class Html;
class HtmlService;
} // namespace odr

namespace odr::internal::html {

HtmlService translate_document(const Document &document);

Html translate_document(const Document &document,
                        const std::string &output_path,
                        const HtmlConfig &config);

} // namespace odr::internal::html

#endif // ODR_INTERNAL_HTML_DOCUMENT_HPP
