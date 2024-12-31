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

HtmlService translate_document(const PopplerPdfFile &pdf_file);

Html translate_poppler_pdf_file(const PopplerPdfFile &pdf_file,
                                const std::string &output_path,
                                const HtmlConfig &config);

class DocumentCopyProtectedException : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

} // namespace odr::internal::html
