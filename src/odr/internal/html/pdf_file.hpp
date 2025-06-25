#pragma once

#include <memory>
#include <string>

namespace odr {
class PdfFile;
struct HtmlConfig;
class HtmlService;
class Logger;
} // namespace odr

namespace odr::internal::html {

odr::HtmlService create_pdf_service(const PdfFile &pdf_file,
                                    const std::string &cache_path,
                                    HtmlConfig config,
                                    std::shared_ptr<Logger> logger);

}
