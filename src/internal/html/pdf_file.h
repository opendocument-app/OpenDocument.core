#ifndef ODR_INTERNAL_HTML_PDF_FILE_H
#define ODR_INTERNAL_HTML_PDF_FILE_H

#include <string>

namespace odr {
class PdfFile;

struct HtmlConfig;
class Html;
} // namespace odr

namespace odr::internal::html {

Html translate_pdf_file(const PdfFile &pdf_file, const std::string &path,
                        const HtmlConfig &config);

}

#endif // ODR_INTERNAL_HTML_PDF_FILE_H
