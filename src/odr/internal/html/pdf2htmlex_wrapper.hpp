#pragma once

#include <memory>
#include <string>

namespace odr {
struct HtmlConfig;
class Html;
class HtmlService;
class Logger;
} // namespace odr

namespace odr::internal {
class PopplerPdfFile;
} // namespace odr::internal

namespace odr::internal::html {

HtmlService create_poppler_pdf_service(const PopplerPdfFile &pdf_file,
                                       const std::string &cache_path,
                                       HtmlConfig config,
                                       std::shared_ptr<Logger> logger);

} // namespace odr::internal::html
