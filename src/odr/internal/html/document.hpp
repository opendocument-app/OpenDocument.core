#pragma once

#include <string>

namespace odr {
class Document;
struct HtmlConfig;
class Html;
class HtmlService;
} // namespace odr

namespace odr::internal::html {

odr::HtmlService create_document_service(const Document &document,
                                         const std::string &output_path,
                                         const HtmlConfig &config);

Html translate_document(const Document &document,
                        const std::string &output_path,
                        const HtmlConfig &config);

} // namespace odr::internal::html
