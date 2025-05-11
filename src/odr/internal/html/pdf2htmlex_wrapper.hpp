#pragma once

#include <stdexcept>
#include <string>

namespace odr {
struct HtmlConfig;
class Html;
class HtmlService;
} // namespace odr

namespace odr::internal {
class PopplerPdfFile;
} // namespace odr::internal

namespace odr::internal::html {

odr::HtmlService create_poppler_pdf_service(const PopplerPdfFile &pdf_file,
                                            const std::string &cache_path,
                                            HtmlConfig config);

} // namespace odr::internal::html
