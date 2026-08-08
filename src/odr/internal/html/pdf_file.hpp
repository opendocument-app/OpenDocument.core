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

HtmlService create_pdf_service(const PdfFile &pdf_file, HtmlConfig config,
                               const Logger &logger);

}
