#pragma once

#include <memory>
#include <string>

namespace odr {
class Document;
struct HtmlConfig;
class HtmlService;
class Logger;
} // namespace odr

namespace odr::internal::html {

HtmlService create_document_service(const Document &document,
                                    const std::string &cache_path,
                                    HtmlConfig config,
                                    std::shared_ptr<Logger> logger);

} // namespace odr::internal::html
