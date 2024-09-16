#ifndef ODR_INTERNAL_HTML_PDF_POPPLER_FILE_HPP
#define ODR_INTERNAL_HTML_PDF_POPPLER_FILE_HPP

#include <string>

namespace odr {
class PopplerPdfFile;

struct HtmlConfig;
class Html;
} // namespace odr

namespace odr::internal::html {

Html translate_pdf_poppler_file(const PopplerPdfFile &pdf_file,
                                const std::string &output_path,
                                const HtmlConfig &config);

}

#endif // ODR_INTERNAL_HTML_PDF_POPPLER_FILE_HPP
