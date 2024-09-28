#ifndef ODR_INTERNAL_HTML_PDF2HTMLEX_WRAPPER_HPP
#define ODR_INTERNAL_HTML_PDF2HTMLEX_WRAPPER_HPP

#include <string>

namespace odr {
struct HtmlConfig;
class Html;
} // namespace odr

namespace odr::internal {
class PopplerPdfFile;
} // namespace odr::internal

namespace odr::internal::html {

Html translate_poppler_pdf_file(const PopplerPdfFile &pdf_file,
                                const std::string &output_path,
                                const HtmlConfig &config);

class DocumentCopyProtectedException : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

} // namespace odr::internal::html

#endif // ODR_INTERNAL_HTML_PDF2HTMLEX_WRAPPER_HPP
