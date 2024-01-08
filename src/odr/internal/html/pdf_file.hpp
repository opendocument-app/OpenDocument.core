#ifndef ODR_INTERNAL_PDF_FILE_HPP
#define ODR_INTERNAL_PDF_FILE_HPP

#include <string>

namespace odr {
class PdfFile;

struct HtmlConfig;
class Html;
} // namespace odr

namespace odr::internal::html {

Html translate_pdf_file(const PdfFile &pdf_file, const std::string &output_path,
                        const HtmlConfig &config);

}

#endif // ODR_INTERNAL_PDF_FILE_HPP
